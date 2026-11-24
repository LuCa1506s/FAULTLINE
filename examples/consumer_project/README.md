# Independent FAULTLINE consumer

Copy this project and the correct 0.1.0b2 wheel to a directory outside the
engine checkout. Create a clean venv and install the wheel with its `gym`
extra. The consumer uses the installed package and runs replay in new processes.

```sh
python -m venv .venv
```

Activate the environment and replace `WHEEL_PATH` with the actual wheel path.

```sh
python -m pip install 'WHEEL_PATH[gym]'
python -I review.py output
```

The project runs a planning benchmark and exports JSON, CSV and replay traces.
Its own policy handles the public position and goal fields. It also runs an
idle policy to demonstrate failed-episode analysis. The summary includes
reason, progress, tick and replay agreement for the failure.
