"""Provider-independent decision policy for structured FAULTLINE observations."""

from collections import deque
from dataclasses import dataclass, field
from hashlib import sha256
import json
import subprocess
import sys
from time import perf_counter
from typing import Protocol
from urllib.parse import urlsplit

from . import Action
from .replay import _unique_object

_PUBLIC_FIELDS = {"position", "goal", "road_axes", "closures", "stops_completed", "stops_total", "remaining_decisions"}
_SYSTEM = 'Choose one action from 0 idle, 1 east, 2 west, 3 south, 4 north. Return only {"action": integer}.'
_PROVIDERS = {"openai": "/v1/chat/completions", "llama.cpp": "/v1/chat/completions",
              "anthropic": "/v1/messages", "ollama": "/api/chat"}


@dataclass(frozen=True)
class Inference:
    text: str
    input_tokens: int
    output_tokens: int


class Backend(Protocol):
    def complete(self, messages: tuple[dict[str, str], ...], max_output_tokens: int, timeout: float) -> Inference: ...


@dataclass(frozen=True)
class Budget:
    decisions: int = 128
    total_tokens: int = 32768
    output_tokens: int = 48
    context_turns: int = 2
    timeout_seconds: float = 20.0

    def __post_init__(self):
        values = ((self.decisions, 1, 4096), (self.total_tokens, 1, 10_000_000),
                  (self.output_tokens, 1, 1024), (self.context_turns, 0, 16))
        if any(type(value) is not int or not low <= value <= high for value, low, high in values):
            raise ValueError("Invalid number of decisions, tokens or context turns")
        if type(self.timeout_seconds) not in (int, float) or not 0 < self.timeout_seconds <= 120:
            raise ValueError("Inference timeout must be between zero and 120 seconds")


@dataclass(frozen=True)
class InferenceLog:
    decision: int
    outcome: str
    request_sha256: str | None
    input_tokens: int | None
    output_tokens: int | None
    seconds: float
    action: int | None


def _request_body(provider, model, messages, limit):
    if provider == "anthropic":
        return {"model": model, "system": messages[0]["content"], "messages": messages[1:], "max_tokens": limit}
    if provider == "ollama":
        return {"model": model, "messages": messages, "stream": False, "format": "json",
                "options": {"num_predict": limit, "temperature": 0}}
    key = "max_completion_tokens" if provider == "openai" else "max_tokens"
    return {"model": model, "messages": messages, "stream": False, key: limit}


def _decode(provider, payload):
    if provider == "anthropic":
        usage = payload["usage"]
        text = "".join(part["text"] for part in payload["content"] if part["type"] == "text")
        count = usage["input_tokens"] + usage.get("cache_creation_input_tokens", 0) + usage.get("cache_read_input_tokens", 0)
        return Inference(text, count, usage["output_tokens"])
    if provider == "ollama":
        return Inference(payload["message"]["content"], payload["prompt_eval_count"], payload["eval_count"])
    return Inference(payload["choices"][0]["message"]["content"], payload["usage"]["prompt_tokens"],
                     payload["usage"]["completion_tokens"])


@dataclass(frozen=True)
class HttpBackend:
    """Talk to one explicit chat endpoint using a bounded disposable worker."""

    provider: str
    origin: str
    model: str
    allow_remote: bool = False
    api_key: str = field(default="", repr=False)

    def __post_init__(self):
        address = urlsplit(self.origin)
        if (self.provider not in _PROVIDERS or not self.model or len(self.model) > 256 or
                address.scheme not in ("http", "https") or address.hostname is None or
                address.path not in ("", "/") or address.query or address.fragment or
                address.username is not None or address.password is not None):
            raise ValueError("Choose a supported provider, model and HTTP origin")
        if address.hostname not in ("127.0.0.1", "::1") and (not self.allow_remote or address.scheme != "https"):
            raise ValueError("Remote HTTPS endpoints require explicit opt-in")
        _ = address.port

    def complete(self, messages, max_output_tokens, timeout):
        headers = {"Content-Type": "application/json"}
        if self.provider == "anthropic":
            headers["anthropic-version"] = "2023-06-01"
        if self.api_key:
            headers["x-api-key" if self.provider == "anthropic" else "Authorization"] = (
                self.api_key if self.provider == "anthropic" else f"Bearer {self.api_key}")
        envelope = {"url": self.origin.rstrip("/") + _PROVIDERS[self.provider], "headers": headers,
                    "body": _request_body(self.provider, self.model, messages, max_output_tokens), "timeout": timeout}
        result = subprocess.run([sys.executable, "-I", "-m", "faultline_sim.model_worker"],
                                input=json.dumps(envelope).encode(), capture_output=True, timeout=timeout)
        if result.returncode:
            raise RuntimeError("Chat transport failed")
        return _decode(self.provider, json.loads(result.stdout, object_pairs_hook=_unique_object))


class ModelAgent:
    """Accept only a typed action from a response and keep policy context bounded."""

    def __init__(self, backend: Backend, budget: Budget = Budget()) -> None:
        self.backend = backend
        self.budget = budget
        self.history = deque(maxlen=budget.context_turns)
        self.logs: list[InferenceLog] = []
        self.tokens_used = 0
        self.halted = False
        self.skipped_decisions = 0

    def decide(self, observation: dict) -> Action | None:
        if self.halted or len(self.logs) >= self.budget.decisions:
            self.skipped_decisions += 1
            return None
        if set(observation) != _PUBLIC_FIELDS:
            raise ValueError("Observation does not match the public route schema")
        prompt = json.dumps(observation, ensure_ascii=True, allow_nan=False, separators=(",", ":"))
        if len(prompt) > 8192:
            raise ValueError("Public observation exceeds its transport limit")
        remaining = self.budget.total_tokens - self.tokens_used
        history = list(self.history)
        limit = self.budget.output_tokens
        while True:
            messages = ({"role": "system", "content": _SYSTEM},
                        *(message for pair in history for message in pair), {"role": "user", "content": prompt})
            # Reserve serialized prompt bytes and extra per-message framing, then account for reported usage.
            reserve = len(json.dumps(messages, ensure_ascii=True).encode()) + 32 * len(messages)
            if reserve + limit <= remaining or not history:
                break
            history.pop(0)
        decision = len(self.logs) + 1
        if reserve + limit > remaining:
            self.halted = True
            self.logs.append(InferenceLog(decision, "token-reservation", None, None, None, 0.0, None))
            return None
        digest = sha256(json.dumps(messages, sort_keys=True).encode()).hexdigest()
        began = perf_counter()
        action = None
        usage = (None, None)
        try:
            result = self.backend.complete(messages, limit, self.budget.timeout_seconds)
            tokens = (result.input_tokens, result.output_tokens)
            if any(type(value) is not int or value < 0 for value in tokens):
                raise ValueError("Invalid reported token usage")
            usage = tokens
            self.tokens_used += sum(tokens)
            if self.tokens_used > self.budget.total_tokens or result.output_tokens > limit:
                self.halted = True
                outcome = "reported-budget-overrun"
            else:
                outcome = "invalid-action"
                if type(result.text) is str and len(result.text) <= 128:
                    try:
                        parsed = json.loads(result.text, object_pairs_hook=_unique_object)
                    except ValueError:
                        parsed = None
                    if type(parsed) is dict and set(parsed) == {"action"} and type(parsed["action"]) is int and 0 <= parsed["action"] <= 4:
                        action = Action(parsed["action"])
                        outcome = "accepted"
        except (TimeoutError, subprocess.TimeoutExpired):
            outcome = "timeout"
            self.halted = True
        except (OSError, RuntimeError, KeyError, IndexError, TypeError, ValueError):
            outcome = "transport-or-usage-error"
            self.halted = True
        encoded = action.value if action is not None else None
        self.logs.append(InferenceLog(decision, outcome, digest, *usage, perf_counter() - began, encoded))
        if action is not None:
            self.history.append((messages[-1], {"role": "assistant", "content": json.dumps({"action": encoded})}))
        return action

    def __call__(self, observation: dict) -> int:
        action = self.decide(observation)
        return action.value if action is not None else -1
