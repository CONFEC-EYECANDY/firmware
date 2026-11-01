"""Convert ESP-IDF build outputs to a portable, checksummed flash bundle."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess


def package(build, out, variant):
    out.mkdir(parents=True, exist_ok=True)
    args = json.loads((build / "flasher_args.json").read_text())
    files = []
    for offset, source in args["flash_files"].items():
        source = build / source
        destination = out / source.name
        shutil.copy2(source, destination)
        files.append({"offset": offset, "file": destination.name,
                      "sha256": hashlib.sha256(destination.read_bytes()).hexdigest()})
    for elf in build.glob("*.elf"):
        shutil.copy2(elf, out / elf.name)
    manifest = {"schema": 1, "chip": args["extra_esptool_args"]["chip"],
                "variant": variant,
                "commit": subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
                "flash_settings": args["flash_settings"], "files": files}
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--variant", required=True)
    options = parser.parse_args()
    package(options.build, options.out, options.variant)
