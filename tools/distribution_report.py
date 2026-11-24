"""Build a CycloneDX inventory from the qualified installed environment."""

from argparse import ArgumentParser
from hashlib import sha256
import json
from pathlib import Path
from urllib.parse import quote
from zipfile import ZipFile


def python_component(name, version, scope, license_expression=None):
    purl = f"pkg:pypi/{quote(name.lower().replace('_', '-'))}@{quote(version)}"
    component = {"type": "library", "name": name, "version": version, "scope": scope, "bom-ref": purl, "purl": purl}
    if license_expression:
        component["licenses"] = [{"expression": license_expression}]
    return component


def make_sbom(inventory, artifact_root):
    installed = [entry["metadata"] for entry in inventory["installed"]]
    app, = [item for item in installed if item["name"].lower().replace("_", "-") == "faultline-sim"]
    root = python_component("faultline-sim", app["version"], "required", "Apache-2.0")
    root["type"] = "application"
    wheels = sorted(artifact_root.glob("*/*.whl"))
    if not wheels:
        raise ValueError("At least one wheel is required")
    root["properties"] = []
    for wheel in wheels:
        root["properties"].append({"name": f"faultline:artifact:{wheel.name}:sha256",
                                   "value": sha256(wheel.read_bytes()).hexdigest()})
        with ZipFile(wheel) as archive:
            licenses = [name for name in archive.namelist() if name.endswith("pybind11.LICENSE")]
            if len(licenses) != 1:
                raise ValueError(f"Missing bundled pybind11 license in {wheel.name}")
    engine = {"type": "library", "name": "FAULTLINE native engine", "version": "0.1.0", "bom-ref": "faultline-native:0.1.0",
              "scope": "required", "licenses": [{"expression": "Apache-2.0"}]}
    binding = python_component("pybind11", "3.1.0", "required", "BSD-3-Clause")
    components = [engine, binding]
    for item in sorted(installed, key=lambda row: row["name"].lower()):
        if item["name"].lower().replace("_", "-") in {"faultline-sim", "pip", "setuptools"}:
            continue
        components.append(python_component(item["name"], item["version"], "optional", item.get("license_expression")))
    return {"bomFormat": "CycloneDX", "specVersion": "1.5", "version": 1,
            "metadata": {"component": root, "properties": [
                {"name": "faultline:inventory", "value": "Bundled native components and installed qualification extras"},
                {"name": "faultline:build-backend", "value": "scikit-build-core==0.12.2"}]},
            "components": components,
            "dependencies": [{"ref": root["bom-ref"], "dependsOn": [engine["bom-ref"], binding["bom-ref"]]}]}


def main():
    parser = ArgumentParser(description=__doc__)
    parser.add_argument("pip_inspect", type=Path)
    parser.add_argument("artifact_root", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    inventory = json.loads(args.pip_inspect.read_bytes())
    args.output.write_text(json.dumps(make_sbom(inventory, args.artifact_root), indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
