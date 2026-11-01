import importlib.util
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("installer", Path(__file__).resolve().parents[2] / "fw-install.py")
installer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(installer)


class InstallerTests(unittest.TestCase):
    def test_resolves_wheel_from_public_release(self):
        asset = {"name": "eyecandy_fw_cli-0.2.0-py3-none-any.whl", "browser_download_url": "https://example.test/wheel"}
        with patch.object(installer, "fetch", return_value=json.dumps({"assets": [asset]}).encode()) as fetch:
            self.assertEqual(installer.wheel_asset("owner/repo"), asset)
            fetch.assert_called_once_with("https://api.github.com/repos/owner/repo/releases/latest")

    def test_invalid_repository_rejected_before_network(self):
        with patch.object(installer, "fetch") as fetch:
            with self.assertRaises(ValueError):
                installer.wheel_asset("../../wrong")
            fetch.assert_not_called()

    @unittest.skipIf(os.name == "nt", "POSIX shell profiles")
    def test_path_setup_is_idempotent_and_quotes_spaces(self):
        with tempfile.TemporaryDirectory(prefix="fw install ") as tmp:
            root = Path(tmp)
            (root / ".bashrc").write_text("# existing settings\n")
            with patch.object(installer.Path, "home", return_value=root):
                installer.add_path(root / "some path" / "bin")
                installer.add_path(root / "some path" / "bin")
            text = (root / ".bashrc").read_text()
            self.assertTrue(text.startswith("# existing settings"))
            self.assertEqual(text.count("# eyecandy-fw installer"), 1)
            self.assertIn("'", (root / ".config/eyecandy-fw/env").read_text())

    def test_local_install_no_path_preserves_user_environment(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            wheel = root / "eyecandy_fw_cli-0.2.0-py3-none-any.whl"
            wheel.touch()
            args = ["fw-install.py", "--prefix", str(root / "install"), "--wheel", str(wheel), "--no-path", "--no-deps"]
            with patch.object(installer.sys, "argv", args), patch.object(installer.venv, "EnvBuilder") as builder, patch.object(installer.subprocess, "run") as run, patch.object(installer, "fetch") as fetch, patch.object(installer, "add_path") as add_path:
                installer.main()
            builder.return_value.create.assert_called_once()
            fetch.assert_not_called()
            add_path.assert_not_called()
            self.assertIn("--force-reinstall", run.call_args_list[0].args[0])
            self.assertEqual(run.call_args_list[1].args[0][-3:], ["config", "--repo", "TL101GRP/firmware"])

    def test_termux_installs_android_extra_and_uses_system_packages(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            wheel = root / "eyecandy_fw_cli-0.3.0-py3-none-any.whl"
            wheel.touch()
            args = ["fw-install.py", "--prefix", str(root / "install"), "--wheel", str(wheel), "--no-path"]
            with patch.dict(os.environ, {"TERMUX_VERSION": "test"}), patch.object(installer.sys, "argv", args), patch.object(installer.venv, "EnvBuilder") as builder, patch.object(installer.subprocess, "run") as run:
                installer.main()
            self.assertTrue(builder.call_args.kwargs["system_site_packages"])
            self.assertEqual(run.call_args_list[0].args[0][-1], str(wheel.resolve()) + "[android]")


if __name__ == "__main__":
    unittest.main()
