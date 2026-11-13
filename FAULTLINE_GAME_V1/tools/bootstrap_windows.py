"""Fetch a local-only Windows toolchain. All artifacts stay below repository .tools."""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import hashlib
import io
import json
import subprocess
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
LOCAL = ROOT / ".tools"


def fetch(url):
    with urllib.request.urlopen(url, timeout=120) as response:
        return response.read()


def ranged_download(url, target, size):
    def part(index):
        start = index * 4_194_304
        end = min(start + 4_194_304, size) - 1
        request = urllib.request.Request(url, headers={"Range": f"bytes={start}-{end}"})
        with urllib.request.urlopen(request, timeout=120) as response:
            data = response.read()
            if response.status != 206 or len(data) != end - start + 1:
                raise RuntimeError("Server did not honor download range")
            return data

    with ThreadPoolExecutor(max_workers=12) as pool:
        parts = list(pool.map(part, range((size + 4_194_303) // 4_194_304)))
    target.write_bytes(b"".join(parts))


def wheel(project, version):
    metadata = json.loads(fetch(f"https://pypi.org/pypi/{project}/{version}/json"))
    asset = next(item for item in metadata["urls"] if item["filename"].endswith("win_amd64.whl"))
    target = LOCAL / asset["filename"]
    ranged_download(asset["url"], target, asset["size"])
    data = target.read_bytes()
    if hashlib.sha256(data).hexdigest() != asset["digests"]["sha256"]:
        raise RuntimeError("Wheel checksum mismatch")
    with zipfile.ZipFile(io.BytesIO(data)) as archive:
        archive.extractall(LOCAL / "python")


if __name__ == "__main__":
    LOCAL.mkdir(exist_ok=True)
    with ThreadPoolExecutor(max_workers=3) as pool:
        jobs = [pool.submit(wheel, "cmake", "4.4.4"), pool.submit(wheel, "ninja", "1.13.2")]
        compiler = LOCAL / "w64devkit-portable.exe"
        release = json.loads(fetch("https://api.github.com/repos/skeeto/w64devkit/releases/tags/v2.10.1"))
        asset = next(item for item in release["assets"] if item["name"] == "w64devkit-x64-2.10.1.7z.exe")
        ranged_download(asset["browser_download_url"], compiler, asset["size"])
        if asset.get("digest"):
            expected = asset["digest"].removeprefix("sha256:")
            if hashlib.sha256(compiler.read_bytes()).hexdigest() != expected:
                raise RuntimeError("Compiler checksum mismatch")
        subprocess.run([str(compiler), "-y", f"-o{LOCAL}"], check=True, cwd=ROOT)
        for job in jobs:
            job.result()
    print("Local toolchain ready in .tools")
