# Model policy integration

`ModelAgent` is a policy for the route-challenges protocol. Its `decide` method
returns a native `Action` or `None` and its callable adapter feeds the scientific
runner. A failed decision is recorded as invalid and consumes an idle action.
The adapter sends only the public route observation and a bounded number of
previous public observations and accepted actions. A response must contain
one JSON field `action` holding a legal integer. The protocol exposes no tools,
reward internals, unannounced closures, seeds, snapshots or file paths.

The `Backend` interface returns text and token usage. The model policy limits
decisions, framed input reservation, total reported tokens, output tokens and
history length. The input reservation uses UTF-8 bytes plus message framing
before each request. Provider token accounting determines actual consumption.
An unknown usage outcome or a reported overrun stops additional inference.
Logs store request digests, timing, token usage, response status and accepted
actions. The host retains credentials and inference logs. The existing Python
policy callback interface remains part of the trusted host boundary.

`HttpBackend` implements a disposable HTTP worker with a hard parent process
deadline, bounded response size and explicit endpoint selection. Loopback
requires a numeric local address. Remote HTTPS requires explicit opt-in and
optional credentials supplied by the caller. Redirects and ambient proxies are
disabled. A separate model server inherits its deployment's access controls.

Supported wire formats are OpenAI-compatible Chat Completions, Anthropic
Messages, Ollama chat and llama.cpp Chat Completions. llama.cpp and other local
servers can be configured as an OpenAI-compatible origin with the corresponding
API profile. Reference documentation follows. [OpenAI Chat](https://developers.openai.com/api/reference/resources/chat) · [Anthropic Messages](https://platform.claude.com/docs/en/api/messages) · [Ollama chat](https://docs.ollama.com/api/chat) · [llama.cpp server](https://github.com/ggml-org/llama.cpp/tree/master/tools/server)

Run `examples/model_agent.py --help` for the local example. Replay works from
the saved actions through `python -m faultline_sim.scientific replay TRACE`.
The example requires the model version and quantization for provenance.

The initial 0.1.0b2 integration used loopback fixtures with synthetic text.
`REAL_LLM_EVALUATION_NOT_VERIFIED` was the verdict for that investigation.
The release evidence tracks real inference separately from the protocol tests.

The associated wheels passed 37 installed tests on Windows CPython 3.12
(10.83 s) and Linux CPython 3.14 (6.34 s). Loopback tests verified all four
wire formats, the process deadline, accepted and invalid actions, budget
limits and replay without a provider call.

## Repeat a local experiment

Use a dedicated working directory and install the exact release wheel with
the dependencies pinned in `requirements/qualification.txt`. The
[release qualification](RELEASE_QUALIFICATION.md) identifies the interpreter,
wheel, GGUF file and tool versions for each measured run. The release evidence
archive includes the bounded runner `model/evaluate.py` and the recorded trace.

For the SmolLM2 setup, obtain the CPU build of llama.cpp b8500 and the
`SmolLM2-135M-Instruct-Q4_K_M.gguf` file at revision
`09816acd5d99df7be770d85ea30822623dab342c`. Check their SHA-256 digests against
the evidence before starting the server. Model files and executables are
downloaded separately from the package. [llama.cpp b8500](https://github.com/ggml-org/llama.cpp/releases/tag/b8500) · [SmolLM2 model](https://huggingface.co/HuggingFaceTB/SmolLM2-135M-Instruct) · [Pinned GGUF](https://huggingface.co/bartowski/SmolLM2-135M-Instruct-GGUF/tree/09816acd5d99df7be770d85ea30822623dab342c)

```sh
llama-server -m SmolLM2-135M-Instruct-Q4_K_M.gguf --alias SmolLM2-135M-Instruct \
  --host 127.0.0.1 --port 18483 -ngl 0 -t 2 -tb 2 -c 2048 -np 1 --temp 0 --seed 42
```

In a second terminal with your clean environment active, run the bounded
evaluation. Stop the server in its own terminal before running the replay.

```sh
mkdir -p results
python -I model/evaluate.py SmolLM2-135M-Instruct 09816acd5d99df7be770d85ea30822623dab342c 18483 results
python -I -m faultline_sim.scientific replay results/trace.json
```

For a separate experiment using llama.cpp's JSON response schema, create a new
output directory and append `--schema --decisions 120` to the experiment command.
The schema describes the five legal actions. All generated actions still pass
through `ModelAgent` validation. [llama.cpp Chat Completions](https://github.com/ggml-org/llama.cpp/blob/b8500/tools/server/README.md#post-v1chatcompletions-openai-compatible-chat-completions-api)

Use the trace and dependency fingerprint from the same run for replay. A new
inference run measures model behavior separately from native replay agreement.
