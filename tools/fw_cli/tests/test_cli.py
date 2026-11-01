import hashlib
import io
import json
from pathlib import Path
import tempfile
import threading
import unittest
from unittest.mock import patch
import zipfile
from urllib.error import HTTPError, URLError
from serial.tools.list_ports_common import ListPortInfo

from fw_cli import cli


class ClientTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.home = Path(self.tmp.name)
        self.patcher = patch.object(cli, "HOME", self.home)
        self.patcher.start()
        self.addCleanup(self.patcher.stop)
        self.bundle = self.home / "bundle"
        self.bundle.mkdir()
        (self.bundle / "app.bin").write_bytes(b"firmware")
        self.manifest = {"schema": 1, "chip": "esp32c3", "commit": "abc", "variant": "a",
                         "flash_settings": {"flash_mode": "dio", "flash_freq": "80m", "flash_size": "2MB"},
                         "files": [{"file": "app.bin", "offset": "0x10000",
                                    "sha256": hashlib.sha256(b"firmware").hexdigest()}]}
        cli.atomic_json(self.bundle / "manifest.json", self.manifest)
        cli.atomic_json(self.home / "current.json", {"directory": "bundle"})

    def test_corrupt_bundle_rejected_before_flash(self):
        (self.bundle / "app.bin").write_bytes(b"corrupt")
        with patch.object(cli.subprocess, "run") as run:
            with self.assertRaisesRegex(ValueError, "Checksum"):
                cli.flash({"port": "TEST", "variant": "a"})
            run.assert_not_called()

    def test_failed_update_preserves_current(self):
        archive = io.BytesIO()
        with zipfile.ZipFile(archive, "w") as z:
            z.writestr("../escape.bin", b"bad")
        release = {"tag_name": "build-1", "assets": [{"name": "firmware-esp32c3-a.zip", "browser_download_url": "https://example.test/firmware.zip"}]}
        with patch.object(cli, "latest_release", return_value=release), patch.object(cli, "download", return_value=archive.getvalue()):
            with self.assertRaisesRegex(ValueError, "Unexpected path"):
                cli.update(cli.DEFAULTS | {"variant": "a"})
        self.assertEqual(cli.current()[1]["commit"], "abc")
        self.assertFalse((self.home / "escape.bin").exists())

    def test_flash_uses_manifest_and_manual_reset(self):
        with patch.object(cli.subprocess, "run") as run:
            cli.flash({"port": "socket://localhost:8888", "manual_reset": True, "variant": "a"})
        command = run.call_args.args[0]
        self.assertIn("no-reset", command)
        self.assertEqual(command[-2:], ["0x10000", str(self.bundle / "app.bin")])
        self.assertNotIn("erase-flash", command)

    def test_successful_update_switches_to_validated_bundle(self):
        archive = io.BytesIO()
        with zipfile.ZipFile(archive, "w") as z:
            for path in self.bundle.iterdir():
                z.write(path, path.name)
        release = {"tag_name": "build-1", "assets": [{"name": "firmware-esp32c3-a.zip", "browser_download_url": "https://example.test/firmware.zip"}]}
        with patch.object(cli, "latest_release", return_value=release), patch.object(cli, "download", return_value=archive.getvalue()):
            cli.update(cli.DEFAULTS | {"variant": "a"})
        root, manifest = cli.current()
        self.assertNotEqual(root, self.bundle)
        self.assertEqual(manifest, self.manifest)
        self.assertTrue(self.bundle.exists())

    def test_existing_monitor_is_reused(self):
        key = hashlib.sha256(b"TEST").hexdigest()[:24]
        with cli.FileLock(str(self.home / f"{key}.monitor.lock")):
            with patch.object(cli, "_monitor") as monitor:
                cli.monitor({"port": "TEST"})
                monitor.assert_not_called()

    def test_missing_release_asset_preserves_cache(self):
        with patch.object(cli, "latest_release", return_value={"assets": []}):
            with self.assertRaisesRegex(ValueError, "unavailable"):
                cli.update(cli.DEFAULTS | {"variant": "a"})
        self.assertEqual(cli.current()[0], self.bundle)

    def test_public_release_lookup_needs_no_subprocess_or_auth(self):
        response = {"tag_name": "fw-123", "assets": []}
        with patch.object(cli, "download", return_value=json.dumps(response).encode()) as download, patch.object(cli.subprocess, "check_output") as process:
            self.assertEqual(cli.latest_release("owner/repo"), response)
            download.assert_called_once_with("https://api.github.com/repos/owner/repo/releases/latest")
            process.assert_not_called()

    def test_download_errors_are_actionable(self):
        for error, message in [
            (HTTPError("https://example.test", 404, "missing", {}, None), "Public release not found"),
            (HTTPError("https://example.test", 403, "denied", {}, None), "rate limit"),
            (URLError("offline"), "offline"),
        ]:
            with self.subTest(error=error), patch.object(cli, "urlopen", side_effect=error):
                with self.assertRaisesRegex(ValueError, message):
                    cli.download("https://example.test")

    def test_download_has_no_authorization_header(self):
        with patch.object(cli, "urlopen") as open_url:
            open_url.return_value.__enter__.return_value.read.return_value = b"public asset"
            self.assertEqual(cli.download("https://example.test"), b"public asset")
        request = open_url.call_args.args[0]
        self.assertFalse(request.has_header("Authorization"))

    def test_selection_persists_without_download(self):
        with patch.object(cli.sys, "argv", ["fw", "config", "b"]), patch.object(cli, "download") as download:
            cli.main()
        self.assertEqual(cli.config()["variant"], "b")
        download.assert_not_called()

    def test_switching_variant_blocks_wrong_cached_flash(self):
        with patch.object(cli.subprocess, "run") as run:
            with self.assertRaisesRegex(ValueError, "another configuration"):
                cli.flash({"port": "TEST", "variant": "b"})
        run.assert_not_called()

    def test_bundle_with_wrong_variant_is_rejected(self):
        archive = io.BytesIO()
        with zipfile.ZipFile(archive, "w") as z:
            for path in self.bundle.iterdir():
                z.write(path, path.name)
        release = {"tag_name": "fw-123", "assets": [{"name": "firmware-esp32c3-b.zip", "browser_download_url": "https://example.test/b.zip"}]}
        with patch.object(cli, "latest_release", return_value=release), patch.object(cli, "download", return_value=archive.getvalue()):
            with self.assertRaisesRegex(ValueError, "does not match"):
                cli.update(cli.DEFAULTS | {"variant": "b"})
        self.assertEqual(cli.current()[0], self.bundle)

    def test_no_implicit_original_configuration(self):
        with patch.object(cli, "download") as download:
            with self.assertRaisesRegex(ValueError, "Choose a configuration"):
                cli.update(cli.DEFAULTS)
        download.assert_not_called()

    def test_port_listing_marks_candidates_without_opening_or_saving(self):
        native = ListPortInfo("COM3")
        native.vid, native.pid = 0x303A, 0x1001
        native.description = "USB JTAG/serial"
        bridge = ListPortInfo("COM4")
        bridge.vid, bridge.pid = 0x10C4, 0xEA60
        unknown = ListPortInfo("COM5")
        cli.atomic_json(self.home / "config.json", {"port": "COM4"})
        before = (self.home / "config.json").read_bytes()
        output = io.StringIO()
        with patch.object(cli.sys, "argv", ["fw", "port"]), patch.object(cli.sys, "stdout", output), patch.object(cli.list_ports, "comports", return_value=[unknown, bridge, native]), patch.object(cli, "is_termux", return_value=False), patch.object(cli.serial, "serial_for_url") as open_port:
            cli.main()
        self.assertIn("COM3 — Likely ESP", output.getvalue())
        self.assertIn("COM4 [selected] — Possible ESP", output.getvalue())
        self.assertIn("COM5 — Other/unknown", output.getvalue())
        open_port.assert_not_called()
        self.assertEqual(before, (self.home / "config.json").read_bytes())

    def test_port_argument_still_saves_selection(self):
        with patch.object(cli.sys, "argv", ["fw", "port", "COM7"]):
            cli.main()
        self.assertEqual(cli.config()["port"], "COM7")

    def test_empty_port_list_preserves_saved_bridge_url(self):
        output = io.StringIO()
        with patch.object(cli.list_ports, "comports", return_value=[]), patch.object(cli, "is_termux", return_value=False), patch.object(cli.sys, "stdout", output):
            cli.show_ports({"port": "socket://localhost:8888"})
        self.assertIn("No serial ports", output.getvalue())
        self.assertIn("socket://localhost:8888", output.getvalue())

    def test_termux_usb_is_not_misrepresented_as_a_serial_port(self):
        output = io.StringIO()
        with patch.object(cli.shutil, "which", return_value="/bin/termux-usb"), patch.object(cli.subprocess, "run") as run, patch.object(cli.sys, "stdout", output):
            run.return_value.stdout = '["/dev/bus/usb/001/002"]'
            cli.show_termux_usb()
        self.assertEqual(run.call_args.args[0], ["/bin/termux-usb", "-l"])
        self.assertIn("ESP identity not determined", output.getvalue())
        self.assertIn("Flash/monitor will request USB permission", output.getvalue())

    def test_termux_timeout_returns_helpful_message(self):
        output = io.StringIO()
        with patch.object(cli.shutil, "which", return_value="/bin/termux-usb"), patch.object(cli.subprocess, "run", side_effect=cli.subprocess.TimeoutExpired("termux-usb", 10)), patch.object(cli.sys, "stdout", output):
            cli.show_termux_usb()
        self.assertIn("listing unavailable", output.getvalue())

    def test_android_flash_handoff_holds_port_lock(self):
        cfg = {"port": "/dev/bus/usb/001/002", "variant": "a"}
        def worker(task, settings, home):
            self.assertEqual(task, "flash")
            _, access = cli.locks(settings["port"])
            with self.assertRaises(cli.Timeout):
                access.acquire(timeout=0)
        with patch.object(cli, "is_termux", return_value=True), patch.object(cli, "run_usb_task", side_effect=worker) as run:
            cli.flash(cfg)
        run.assert_called_once_with("flash", cfg, self.home)

    def test_android_monitor_dispatches_usb_worker(self):
        cfg = {"port": "/dev/bus/usb/001/002", "baud": 115200}
        with patch.object(cli, "is_termux", return_value=True), patch.object(cli, "run_usb_task") as run, patch.object(cli.serial, "serial_for_url") as serial_open:
            cli.monitor(cfg)
        run.assert_called_once_with("monitor", cfg, self.home)
        serial_open.assert_not_called()

    def test_android_disconnect_exits_instead_of_reopening_stale_fd(self):
        with self.assertRaisesRegex(cli.serial.SerialException, "disconnected"):
            cli._monitor({"port": "/dev/bus/usb/001/002", "baud": 115200},
                         opener=lambda *a, **k: (_ for _ in ()).throw(cli.serial.SerialException("disconnected")),
                         reconnect=False)

    def test_monitor_releases_port_for_flash_and_reconnects(self):
        flashed = threading.Event()
        started = threading.Event()
        errors = []
        opens = []

        class Stream:
            def __enter__(self):
                opens.append(1)
                if len(opens) == 2:
                    raise KeyboardInterrupt
                started.set()
                return self
            def __exit__(self, *args):
                pass
            def read(self, count):
                # Wait until the flasher holds the intent lock.
                intent, _ = cli.locks("TEST")
                for _ in range(200):
                    try:
                        with intent.acquire(timeout=0):
                            pass
                    except cli.Timeout:
                        return b""
                    cli.time.sleep(0.01)
                raise AssertionError("Flasher did not request the port")

        def worker():
            try:
                if not started.wait(3):
                    raise AssertionError("Monitor did not start")
                cli.flash({"port": "TEST", "variant": "a"})
                flashed.set()
            except Exception as exc:
                errors.append(exc)

        with patch.object(cli.serial, "serial_for_url", side_effect=lambda *a, **k: Stream()), patch.object(cli.subprocess, "run"):
            thread = threading.Thread(target=worker, daemon=True)
            thread.start()
            with self.assertRaises(KeyboardInterrupt):
                cli.monitor({"port": "TEST", "baud": 115200})
            thread.join(3)
        self.assertEqual(errors, [])
        self.assertTrue(flashed.is_set())
        self.assertEqual(len(opens), 2)


if __name__ == "__main__":
    unittest.main()
