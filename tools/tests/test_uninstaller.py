import importlib.util
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("uninstaller", ROOT / "fw-uninstall.py")
uninstaller = importlib.util.module_from_spec(spec)
spec.loader.exec_module(uninstaller)
spec2 = importlib.util.spec_from_file_location("installer_for_uninstall", ROOT / "fw-install.py")
installer = importlib.util.module_from_spec(spec2)
spec2.loader.exec_module(installer)


class UninstallerTests(unittest.TestCase):
    def test_unmanaged_install_is_not_removed(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "venv").mkdir()
            with patch.object(uninstaller.sys, "argv", ["uninstall", "--prefix", tmp]):
                with self.assertRaisesRegex(ValueError, "No managed"):
                    uninstaller.main()
            self.assertTrue((root / "venv").exists())

    def test_managed_install_and_data_removed_but_unrelated_files_kept(self):
        for keep_data in (False, True):
            with self.subTest(keep_data=keep_data), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                prefix, data = root / "install", root / "data"
                (prefix / "venv").mkdir(parents=True)
                data.mkdir()
                (data / "config.json").write_text("{}")
                (data / "build-test").mkdir()
                (data / "build-test/manifest.json").write_text("{}")
                (data / "unrelated.txt").write_text("preserve")
                (prefix / "other.txt").write_text("preserve")
                (prefix / "install-state.json").write_text(json.dumps({"product": "eyecandy-fw", "data_directory": str(data), "path_setup": False}))
                args = ["uninstall", "--prefix", str(prefix)] + (["--keep-data"] if keep_data else [])
                with patch.object(uninstaller.sys, "argv", args):
                    uninstaller.main()
                self.assertFalse((prefix / "venv").exists())
                self.assertFalse((prefix / "install-state.json").exists())
                self.assertTrue((prefix / "other.txt").exists())
                self.assertTrue((data / "unrelated.txt").exists())
                self.assertEqual((data / "config.json").exists(), keep_data)
                self.assertEqual((data / "build-test").exists(), keep_data)

    @unittest.skipIf(os.name == "nt", "POSIX profile cleanup")
    def test_path_cleanup_preserves_existing_profile_content(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            prefix = root / "install"
            (root / ".bashrc").write_text("# user content\n")
            with patch.object(Path, "home", return_value=root):
                installer.add_path(prefix / "venv/bin")
                uninstaller.remove_path(prefix, {})
            self.assertEqual((root / ".bashrc").read_text().strip(), "# user content")
            self.assertFalse((root / ".config/eyecandy-fw/env").exists())


if __name__ == "__main__":
    unittest.main()
