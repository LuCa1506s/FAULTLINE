import json
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from threading import Thread
from time import sleep

import pytest

from faultline_sim import Action
from faultline_sim.model import Budget, HttpBackend, Inference, ModelAgent
from faultline_sim.qualification import RouteChallenge
from faultline_sim.scientific import run_episode, verify_episode


@pytest.fixture
def observation():
    env = RouteChallenge("adaptation", "EASY", 14)
    try:
        yield env.observation()
    finally:
        env.close()


def test_decisions_context_and_validation(observation):
    class Fixture:
        def __init__(self):
            self.received = []

        def complete(self, messages, limit, timeout):
            self.received.append(messages)
            assert set(json.loads(messages[-1]["content"])) == set(observation)
            text = '{"action":1}' if len(self.received) == 1 else '{"action":2,"action":1}'
            return Inference(text, 100, 5)

    fixture = Fixture()
    agent = ModelAgent(fixture, Budget(decisions=2, context_turns=1))
    with pytest.raises(ValueError, match="public"):
        agent.decide({**observation, "seed": 14})
    assert agent.decide(observation) is Action.EAST
    assert agent(observation) == -1
    assert agent(observation) == -1
    assert len(fixture.received[1]) == 4
    assert agent.tokens_used == 210 and agent.skipped_decisions == 1
    assert [record.outcome for record in agent.logs] == ["accepted", "invalid-action"]
    exhausted = ModelAgent(fixture, Budget(total_tokens=1))
    assert exhausted(observation) == -1 and exhausted.logs[-1].outcome == "token-reservation"
    with pytest.raises(ValueError, match="opt-in"):
        HttpBackend("openai", "https://example.invalid", "fixture")


@pytest.fixture
def loopback():
    received = []
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass

        def do_POST(self):
            body = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
            received.append((self.path, body))
            action = '{"action":1}'
            if body["model"] == "slow-fixture":
                sleep(2)
            if self.path == "/api/chat":
                result = {"message": {"content": action}, "prompt_eval_count": 90, "eval_count": 5}
            elif self.path == "/v1/messages":
                result = {"content": [{"type": "text", "text": action}], "usage": {"input_tokens": 90, "output_tokens": 5}}
            else:
                result = {"choices": [{"message": {"content": action}}],
                          "usage": {"prompt_tokens": 90, "completion_tokens": 5}}
            try:
                self.send_response(200)
                self.end_headers()
                self.wfile.write(json.dumps(result).encode())
            except OSError:
                pass

    with ThreadingHTTPServer(("127.0.0.1", 0), Handler) as server:
        thread = Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            yield f"http://127.0.0.1:{server.server_port}", received
        finally:
            server.shutdown()
            thread.join()


@pytest.mark.parametrize("provider,endpoint,limit_key", [
    ("openai", "/v1/chat/completions", "max_completion_tokens"),
    ("llama.cpp", "/v1/chat/completions", "max_tokens"),
    ("anthropic", "/v1/messages", "max_tokens"),
    ("ollama", "/api/chat", "options"),
])
def test_all_http_wire_formats(loopback, observation, provider, endpoint, limit_key):
    host, received = loopback
    agent = ModelAgent(HttpBackend(provider, host, "fixture"), Budget(decisions=1))
    assert agent.decide(observation) is Action.EAST
    path, body = received[0]
    assert path == endpoint and limit_key in body
    assert "tools" not in body
    assert set(json.loads(body["messages"][-1]["content"])) == set(observation)
    assert agent.tokens_used == 95 and len(agent.logs) == 1


def test_process_deadline_and_inference_free_replay(loopback, observation):
    host, received = loopback
    slow = ModelAgent(HttpBackend("ollama", host, "slow-fixture"), Budget(timeout_seconds=0.7))
    assert slow(observation) == -1 and slow.logs[-1].outcome == "timeout"
    agent = ModelAgent(HttpBackend("llama.cpp", host, "fixture"), Budget(decisions=2))
    trace, _, _ = run_episode("navigation", "EASY", 14, agent)
    calls = len(received)
    assert verify_episode(trace) == trace["steps"][-1]
    assert len(received) == calls and len(agent.logs) == 2
