"""Build only configs/*.hpp; restore main/config.hpp even if a build fails."""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import zipfile

from package_firmware import package

ROOT = Path(__file__).resolve().parents[1]


def configurations(root):
    headers = sorted((root / "configs").glob("*.hpp"))
    if not headers:
        raise ValueError("No configs/*.hpp found; refusing to build the original config.hpp")
    for header in headers:
        if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_-]*", header.stem):
            raise ValueError(f"Invalid config name: {header.name}; use letters, digits, _ or -")
    return headers


def build_all(root=ROOT):
    headers = configurations(root)
    target = root / "main/config.hpp"
    original = target.read_bytes()
    output = root / "dist/firmware"
    output.mkdir(parents=True, exist_ok=True)
    # Assemble in a temporary directory so a failed variant cannot publish a partial set.
    with tempfile.TemporaryDirectory(prefix="fw-variants-") as tmp:
        staging = Path(tmp)
        try:
            for header in headers:
                name = header.stem
                print(f"Building config {name}", flush=True)
                target.write_bytes(header.read_bytes())
                build = root / "build/variants" / name
                subprocess.run(["idf.py", "-B", str(build), "build"], cwd=root, check=True)
                bundle = staging / name
                package(build, bundle, name)
                with zipfile.ZipFile(staging / f"firmware-esp32c3-{name}.zip", "w", zipfile.ZIP_DEFLATED) as archive:
                    for path in bundle.iterdir():
                        archive.write(path, path.name)
        finally:
            target.write_bytes(original)
        for old in output.glob("firmware-esp32c3-*.zip"):
            old.unlink()
        for archive in staging.glob("*.zip"):
            shutil.copy2(archive, output / archive.name)


if __name__ == "__main__":
    build_all()
