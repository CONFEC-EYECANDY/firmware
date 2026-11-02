"""Private callback launched by termux-usb with TERMUX_USB_FD inherited."""
import json
import os
from pathlib import Path
import sys
from types import ModuleType

from . import cli
from .usb_serial import USBSerial


def prepare_android_esptool():
    """Disable desktop TTY discovery in this dedicated Android USB worker.

    esp-pylib imports pySerial's enumerator even when esptool receives an
    already-connected transport. Android has no supported pySerial enumerator;
    USB permission, selection, and VID/PID come from our USBSerial instead.
    """
    if sys.platform != "android":
        return
    module = ModuleType("serial.tools.list_ports")
    module.comports = lambda include_links=False: []
    sys.modules.setdefault(module.__name__, module)


def flash_usb(cfg, fd):
    prepare_android_esptool()
    import esptool
    from esptool.targets.esp32c3 import ESP32C3ROM
    from esptool.reset import ClassicReset, USBJTAGSerialReset

    class AndroidESP32C3(ESP32C3ROM):
        def _construct_reset_strategy_sequence(self, mode):
            # USB fds are not tty fds: never use the POSIX ioctl reset path.
            if self.get_usb_vid_pid() == (0x303A, 0x1001):
                return (USBJTAGSerialReset(self._port),)
            return (ClassicReset(self._port),)

    root, manifest = cli.current()  # revalidate after permission handoff
    if manifest.get("variant") != cli.require_variant(cfg):
        raise ValueError("Cached firmware changed configuration; run fw update again")
    with USBSerial(fd, cfg["port"], baudrate=115200, timeout=3) as stream:
        esp = AndroidESP32C3(stream)
        esp.cache["usb_vid"], esp.cache["usb_pid"] = stream.vid, stream.pid
        esp.connect(mode="no-reset" if cfg.get("manual_reset") else "default-reset")
        # Use esptool's flash/verify/reset pipeline with our already-connected
        # transport. It retains normal chip checks and image validation.
        esptool.main(cli.flash_arguments(cfg, root, manifest), esp=esp)


def main():
    job = json.loads(Path(sys.argv[1]).read_text())
    result = {"ok": False, "error": "USB callback did not complete"}
    try:
        fd = int(os.environ["TERMUX_USB_FD"])
        os.fstat(fd)  # Detect missing/closed descriptors before touching libusb.
        cli.HOME = Path(job["home"])
        cfg = job["config"]
        if job["task"] == "flash":
            flash_usb(cfg, fd)
        elif job["task"] == "monitor":
            def open_usb(port, baudrate, timeout):
                return USBSerial(fd, port, baudrate, timeout, monitor=True)
            cli._monitor(cfg, opener=open_usb, reconnect=False)
        else:
            raise ValueError("Unknown Android USB task")
        result = {"ok": True}
    except KeyboardInterrupt:
        result = {"ok": job["task"] == "monitor", "error": "USB operation interrupted"}
    except BaseException as exc:
        # Record even esptool's nonzero SystemExit for the parent process.
        result = {"ok": False, "error": f"{type(exc).__name__}: {exc}. If the USB path changed, run fw port and select it again."}
    finally:
        Path(job["result"]).write_text(json.dumps(result))


if __name__ == "__main__":
    main()
