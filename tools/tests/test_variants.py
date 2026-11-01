import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
import build_variants


class VariantTests(unittest.TestCase):
    def test_no_headers_never_builds_original(self):
        with tempfile.TemporaryDirectory() as tmp, patch.object(build_variants.subprocess, "run") as run:
            with self.assertRaisesRegex(ValueError, "refusing"):
                build_variants.build_all(Path(tmp))
            run.assert_not_called()

    def test_original_restored_after_failure(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "main").mkdir()
            (root / "configs").mkdir()
            target = root / "main/config.hpp"
            target.write_bytes(b"original\r\n")
            (root / "configs/a.hpp").write_bytes(b"variant a")
            def fail(*args, **kwargs):
                self.assertEqual(target.read_bytes(), b"variant a")
                raise subprocess.CalledProcessError(1, "idf.py")
            with patch.object(build_variants.subprocess, "run", side_effect=fail):
                with self.assertRaises(subprocess.CalledProcessError):
                    build_variants.build_all(root)
            self.assertEqual(target.read_bytes(), b"original\r\n")
            self.assertEqual(list((root / "dist/firmware").glob("*.zip")), [])

    def test_all_headers_built_and_named_without_original(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "main").mkdir()
            (root / "configs").mkdir()
            target = root / "main/config.hpp"
            target.write_bytes(b"original")
            for name in ("a", "b"):
                (root / f"configs/{name}.hpp").write_bytes(name.encode())
            seen = []
            def build(*args, **kwargs):
                seen.append(target.read_bytes())
            def package(build, out, variant):
                self.assertEqual(build.name, variant)
                out.mkdir()
                (out / "manifest.json").write_text(variant)
            with patch.object(build_variants.subprocess, "run", side_effect=build), patch.object(build_variants, "package", side_effect=package):
                build_variants.build_all(root)
            self.assertEqual(seen, [b"a", b"b"])
            self.assertEqual(target.read_bytes(), b"original")
            self.assertEqual(sorted(p.name for p in (root / "dist/firmware").glob("*.zip")), ["firmware-esp32c3-a.zip", "firmware-esp32c3-b.zip"])


if __name__ == "__main__":
    unittest.main()
