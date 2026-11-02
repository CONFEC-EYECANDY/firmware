import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

import espbridge
import usb.core
from esptool.loader import slip_reader
from esptool.targets.esp32c3 import ESP32C3ROM

from fw_cli import android, android_worker, cli
from fw_cli.usb_serial import USBSerial


class FakeDevice:
    def __init__(self, vid=0x303A, pid=0x1001):
        self.idVendor, self.idProduct = vid, pid
        self.controls, self.reads, self.writes = [], [], []
        self.write_chunk = None

    def get_active_configuration(self):
        return [[SimpleNamespace(bEndpointAddress=0x81, wMaxPacketSize=64)]]

    def ctrl_transfer(self, *args, **kwargs):
        self.controls.append(args)
        return 0

    def read(self, *args, **kwargs):
        if self.reads:
            item = self.reads.pop(0)
            if isinstance(item, Exception):
                raise item
            return item
        raise usb.core.USBTimeoutError("timeout")

    def write(self, endpoint, data, **kwargs):
        count = min(len(data), self.write_chunk or len(data))
        self.writes.append(bytes(data[:count]))
        return count


class TransportTests(unittest.TestCase):
    def setUp(self):
        self.device = FakeDevice()
        patches = [patch("espbridge.wrap_fd", side_effect=lambda fd: self.device),
                   patch("espbridge.get_cdc_endpoints", return_value=(0x81, 1, 1)),
                   patch("espbridge.find_cdc_control_interface", return_value=0),
                   patch("usb.util.claim_interface"), patch("usb.util.release_interface"),
                   patch("usb.util.dispose_resources")]
        for p in patches:
            p.start()
            self.addCleanup(p.stop)

    def stream(self, **kwargs):
        stream = USBSerial(42, "/dev/bus/usb/001/002", timeout=0.002, **kwargs)
        self.addCleanup(stream.close)
        return stream

    def test_buffering_and_esptool_slip_decoder(self):
        stream = self.stream()
        self.device.reads = [b"\xc0\x01\xdb", b"\xdc\x02\xc0"]
        packets = slip_reader(stream, lambda *args: None)
        self.assertEqual(next(packets), b"\x01\xc0\x02")

    def test_partial_writes_send_every_byte(self):
        stream = self.stream()
        self.device.write_chunk = 2
        self.assertEqual(stream.write(b"abcdef"), 6)
        self.assertEqual(b"".join(self.device.writes), b"abcdef")

    def test_native_reset_control_targets_cdc_interface(self):
        stream = self.stream(monitor=True)
        stream.setRTS(True)
        self.assertEqual(self.device.controls[-1][:4], (0x21, 0x22, 3, 0))
        stream.setDTR(False)
        self.assertEqual(self.device.controls[-1][:4], (0x21, 0x22, 2, 0))

    def test_cp210_setup_uses_interface_requests_without_reset_pulses(self):
        self.device = FakeDevice(0x10C4, 0xEA60)
        self.stream()
        baud = next(c for c in self.device.controls if c[1] == 0x1E)
        self.assertEqual(baud[0], 0x41)
        self.assertEqual(baud[4], (115200).to_bytes(4, "little"))
        states = [c[2] for c in self.device.controls if c[1] == 0x01]
        self.assertEqual(states, [0x0300])

    def test_ftdi_status_bytes_removed_per_packet(self):
        self.device = FakeDevice(0x0403, 0x6001)
        stream = self.stream()
        self.device.reads = [b"\x01\x60" + b"A" * 62 + b"\x01\x60BC"]
        self.assertEqual(stream.read(64), b"A" * 62 + b"BC")
        with self.assertRaisesRegex(ValueError, "115200"):
            stream.baudrate = 9600

    def test_disconnect_is_not_treated_as_timeout(self):
        stream = self.stream()
        self.device.reads = [usb.core.USBError("device gone")]
        with self.assertRaisesRegex(cli.serial.SerialException, "disconnected"):
            stream.read(1)

    def test_rejects_unsupported_device_before_writing(self):
        self.device = FakeDevice(0xFFFF, 1)
        with self.assertRaisesRegex(cli.serial.SerialException, "Unsupported"):
            self.stream()
        self.assertEqual(self.device.controls, [])

    def test_esptool_receives_connected_usb_transport_and_manifest_offsets(self):
        cfg = {"variant": "a", "port": "/dev/bus/usb/001/002"}
        manifest = {"variant": "a", "chip": "esp32c3", "flash_settings": {
            "flash_mode": "dio", "flash_freq": "80m", "flash_size": "2MB"},
            "files": [{"offset": "0x10000", "file": "app.bin"}]}
        with patch.object(cli, "current", return_value=(Path("/bundle"), manifest)), patch.object(ESP32C3ROM, "connect") as connect, patch("esptool.main") as main:
            android_worker.flash_usb(cfg, 42)
        connect.assert_called_once_with(mode="default-reset")
        esp = main.call_args.kwargs["esp"]
        self.assertEqual(esp.cache["usb_vid"], 0x303A)
        self.assertEqual(type(esp._construct_reset_strategy_sequence("default-reset")[0]).__name__, "USBJTAGSerialReset")
        self.assertEqual(main.call_args.args[0][-2:], ["0x10000", str(Path("/bundle/app.bin"))])


class PermissionTests(unittest.TestCase):
    def test_android_worker_imports_real_esptool_in_fresh_process(self):
        # A fresh interpreter is essential: desktop imports in this test module
        # would otherwise hide pySerial's Android import failure.
        script = '''
import sys
sys.platform = "android"
from pathlib import Path
from unittest.mock import patch
from fw_cli import android_worker, cli
cfg = {"variant": "a", "port": "/dev/bus/usb/001/002"}
manifest = {"variant": "a", "chip": "esp32c3", "flash_settings": {
    "flash_mode": "dio", "flash_freq": "80m", "flash_size": "2MB"},
    "files": [{"offset": "0x10000", "file": "app.bin"}]}
class ReachedUSB(Exception):
    pass
# Exercise the actual flash entry point, including all dependency imports,
# before stopping at the hardware boundary.
with patch.object(cli, "current", return_value=(Path("/bundle"), manifest)), \\
     patch.object(android_worker, "USBSerial", side_effect=ReachedUSB):
    try:
        android_worker.flash_usb(cfg, 42)
    except ReachedUSB:
        pass
    else:
        raise AssertionError("Did not reach USB transport")
import esptool
from esp_pylib.serial_ports import get_port_names
assert get_port_names() == []
assert sys.platform == "android"
'''
        result = subprocess.run([sys.executable, "-c", script],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertNotIn("enumerate ttys", result.stderr + result.stdout)

    def test_callback_preserves_config_and_uses_installed_python(self):
        cfg = {"port": "/dev/bus/usb/001/002", "variant": "b", "manual_reset": True}
        def launch(args, **kwargs):
            self.assertEqual(args[1:4], ["-r", "-E", "-e"])
            wrapper = Path(args[4])
            self.assertIn("fw_cli.android_worker", wrapper.read_text())
            job = json.loads((wrapper.parent / "job.json").read_text())
            self.assertEqual(job["config"], cfg)
            self.assertEqual(job["task"], "flash")
            Path(job["result"]).write_text('{"ok": true}')
        with patch.object(android.shutil, "which", side_effect=lambda name: "/bin/" + name), patch.object(android.subprocess, "run", side_effect=launch):
            android.run_usb_task("flash", cfg, Path("/tmp/fw-state"))

    def test_permission_denial_cannot_report_flash_success(self):
        with patch.object(android.shutil, "which", side_effect=lambda name: "/bin/" + name), patch.object(android.subprocess, "run"):
            with self.assertRaisesRegex(ValueError, "no callback result"):
                android.run_usb_task("flash", {"port": "/dev/bus/usb/001/002"}, Path("/tmp/fw-state"))

    def test_worker_flash_failure_reaches_parent(self):
        def launch(args, **kwargs):
            job = json.loads((Path(args[4]).parent / "job.json").read_text())
            Path(job["result"]).write_text('{"ok": false, "error": "sync failed"}')
        with patch.object(android.shutil, "which", side_effect=lambda name: "/bin/" + name), patch.object(android.subprocess, "run", side_effect=launch):
            with self.assertRaisesRegex(ValueError, "sync failed"):
                android.run_usb_task("flash", {"port": "/dev/bus/usb/001/002"}, Path("/tmp/fw-state"))
