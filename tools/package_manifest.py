"""Create or verify the SHA-256 manifest of a local FAULTLINE release kit."""

from argparse import ArgumentParser
from hashlib import file_digest, sha256
import json
from pathlib import Path
import subprocess
import tomllib


def digest(path: Path) -> str:
    with path.open("rb") as stream:
        return file_digest(stream, "sha256").hexdigest()


def release_entries(directory: Path) -> list[dict[str, str | int]]:
    return [
        {"path": path.relative_to(directory).as_posix(), "bytes": path.stat().st_size, "sha256": digest(path)}
        for path in sorted(directory.rglob("*"), key=lambda entry: entry.relative_to(directory).as_posix())
        if path.is_file() and path.name not in {"manifest.json", "SHA256SUMS"}
    ]


def source_info(repository: Path) -> dict[str, str | None]:
    committed_base = None
    if (repository / ".git").exists():
        try:
            committed_base = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=repository, text=True).strip()
        except (OSError, subprocess.CalledProcessError):
            pass
    source = [repository / rel for rel in ("CMakeLists.txt", "FAULTLINE_GAME_V1/CMakeLists.txt", "pyproject.toml")]
    for root in ("FAULTLINE_GAME_V1/src", "sdk", "python"):
        source.extend(path for path in (repository / root).rglob("*") if path.is_file()
                      and (path.suffix in {".cpp", ".hpp", ".py", ".pyi"} or path.name == "py.typed"))
    source_order = sorted(source, key=lambda path: path.relative_to(repository).as_posix())
    source_lines = "".join(f"{digest(path)}  {path.relative_to(repository).as_posix()}\n" for path in source_order)
    return {"git_base": committed_base, "source_sha256": sha256(source_lines.encode()).hexdigest()}


def write_manifest(directory: Path, repository: Path) -> None:
    files = release_entries(directory)
    wheels = [entry for entry in files if str(entry["path"]).endswith(".whl")]
    if {str(entry["path"]).split("/", 1)[0] for entry in wheels} != {"windows", "linux"}:
        raise ValueError("The kit must contain Windows and Linux wheels in their platform directories")
    metadata = tomllib.loads((repository / "pyproject.toml").read_text(encoding="utf-8"))
    manifest = {"manifest_version": 1, "distribution": metadata["project"]["name"],
                "version": metadata["project"]["version"], "source": source_info(repository),
                "files": files}
    manifest_path = directory / "manifest.json"
    manifest_path.write_bytes((json.dumps(manifest, indent=2) + "\n").encode("utf-8"))
    checksum_lines = [f"{entry['sha256']}  {entry['path']}\n" for entry in files]
    checksum_lines.append(f"{digest(manifest_path)}  manifest.json\n")
    (directory / "SHA256SUMS").write_bytes("".join(checksum_lines).encode("utf-8"))


def verify(directory: Path) -> None:
    manifest = directory / "manifest.json"
    files = release_entries(directory)
    if files != json.loads(manifest.read_bytes())["files"]:
        raise ValueError("Release files differ from the manifest")
    expected = [f"{entry['sha256']}  {entry['path']}" for entry in files]
    expected.append(f"{digest(manifest)}  manifest.json")
    if (directory / "SHA256SUMS").read_text(encoding="utf-8").splitlines() != expected:
        raise ValueError("SHA256SUMS differs from the release contents")


def main() -> None:
    parser = ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--verify", action="store_true")
    args = parser.parse_args()
    try:
        if not args.verify:
            write_manifest(args.directory, Path(__file__).resolve().parent.parent)
        verify(args.directory)
    except (OSError, KeyError, ValueError) as error:
        parser.exit(1, f"{error}\n")
    print(f"Verified {len(release_entries(args.directory))} files in {args.directory}")


if __name__ == "__main__":
    main()
