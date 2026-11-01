"""Android USB permission handoff. The callback uses the same installed Python."""
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import tempfile


def is_usb_path(port):
    return bool(re.fullmatch(r"/dev/bus/usb/\d+/\d+", port))


def run_usb_task(task, cfg, home):
    """Run an operation in a child that inherits Android's permitted USB fd.

    Serial locks belong to the caller for flash, and to the child for monitor.
    A result file distinguishes a completed callback from a permission error
    (some termux-api failures exit successfully without executing the callback).
    """
    try:
        import espbridge  # noqa: F401 -- fail before asking permission if missing
    except ImportError as exc:
        raise ValueError("Android USB dependencies missing. Rerun fw-install in Termux.") from exc
    executable = shutil.which("termux-usb")
    shell = shutil.which("sh")
    if not executable or not shell:
        raise ValueError("Install termux-api and the matching Termux:API app first.")
    if not is_usb_path(cfg["port"]):
        raise ValueError("Choose a USB path from 'fw port' first")
    with tempfile.TemporaryDirectory(prefix="fw-usb-") as tmp:
        root = Path(tmp)
        job, result, callback = root / "job.json", root / "result.json", root / "callback"
        job.write_text(json.dumps({"task": task, "config": cfg, "home": str(home), "result": str(result)}))
        # Only a single executable path is handed to termux-api. All arguments
        # are quoted in our private wrapper, not split by the API's launcher.
        if any(c.isspace() for c in str(callback)):
            raise ValueError("Termux temporary directory must not contain whitespace")
        callback.write_text(f"#!{shell}\nexec {shlex.quote(sys.executable)} -m fw_cli.android_worker {shlex.quote(str(job))}\n")
        callback.chmod(0o700)
        print(f"Opening Android USB device {cfg['port']}; accept the USB permission prompt.", flush=True)
        env = os.environ.copy()
        env.pop("TERMUX_USB_FD", None)
        subprocess.run([executable, "-r", "-E", "-e", str(callback), cfg["port"]], env=env, check=True)
        if not result.exists():
            raise ValueError("USB permission/open failed: no callback result. Check Termux:API, grant permission, and rerun 'fw port' if reconnected.")
        status = json.loads(result.read_text())
        if not status.get("ok"):
            raise ValueError(status.get("error", "Android USB operation failed"))
