"""Firmware client. ESP-IDF is only required on the build runner."""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
import zipfile
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen
import re

from filelock import FileLock, Timeout
import serial
from .android import is_usb_path, run_usb_task

HOME = Path(os.environ.get("FW_HOME", Path.home() / ".eyecandy-fw"))
DEFAULTS = {"repo": "CONFEC-EYECANDY/firmware", "baud": 115200}


def atomic_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(mode="w", dir=path.parent, delete=False) as f:
        json.dump(value, f, indent=2)
        name = f.name
    os.replace(name, path)


def config():
    path = HOME / "config.json"
    return DEFAULTS | (json.loads(path.read_text()) if path.exists() else {})


def download(url):
    if not url.startswith("https://"):
        raise ValueError("Downloads require HTTPS")
    request = Request(url, headers={"User-Agent": "eyecandy-fw", "Accept": "application/vnd.github+json"})
    try:
        with urlopen(request, timeout=60) as response:
            return response.read()
    except HTTPError as exc:
        if exc.code == 404:
            raise ValueError("Public release not found. Wait for the first successful main-branch build; the repository must be public.") from exc
        if exc.code == 403:
            raise ValueError("GitHub denied the request (possibly its anonymous API rate limit). Try again later.") from exc
        raise ValueError(f"GitHub download failed: HTTP {exc.code}") from exc
    except URLError as exc:
        raise ValueError(f"Download failed: {exc.reason}") from exc


def latest_release(repo):
    if not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", repo):
        raise ValueError("Repository must be OWNER/REPO")
    release = json.loads(download(f"https://api.github.com/repos/{repo}/releases/latest"))
    if release.get("draft") or release.get("prerelease"):
        raise ValueError("Release is not published")
    return release


def validate_bundle(root):
    manifest = json.loads((root / "manifest.json").read_text())
    if manifest.get("schema") != 1 or manifest.get("chip") != "esp32c3":
        raise ValueError("Unsupported firmware bundle")
    if not manifest.get("files"):
        raise ValueError("Bundle has no flash files")
    for entry in manifest["files"]:
        name = entry["file"]
        if Path(name).name != name or "\\" in name or name in ("", ".", ".."):
            raise ValueError("Invalid firmware filename")
        if int(entry["offset"], 0) < 0:
            raise ValueError("Invalid flash offset")
        if hashlib.sha256((root / name).read_bytes()).hexdigest() != entry["sha256"]:
            raise ValueError(f"Checksum mismatch: {name}")
    settings = manifest["flash_settings"]
    if any(not isinstance(settings.get(k), str) for k in ("flash_mode", "flash_freq", "flash_size")):
        raise ValueError("Missing flash settings")
    return manifest


def require_variant(cfg):
    name = cfg.get("variant")
    if not name or not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_-]*", name):
        raise ValueError("Choose a configuration first: fw configs, then fw config NAME")
    return name


def available_variants(release):
    variants = {}
    for asset in release["assets"]:
        match = re.fullmatch(r"firmware-esp32c3-([A-Za-z0-9][A-Za-z0-9_-]*)\.zip", asset["name"])
        if match:
            variants[match.group(1)] = asset
    return variants


def list_variants(cfg):
    release = latest_release(cfg["repo"])
    names = sorted(available_variants(release))
    if not names:
        raise ValueError("Latest release contains no named configurations")
    for name in names:
        print(name + (" (selected)" if name == cfg.get("variant") else ""))


def update(cfg):
    variant = require_variant(cfg)
    release = latest_release(cfg["repo"])
    assets = available_variants(release)
    if variant not in assets:
        raise ValueError(f"Configuration {variant!r} is unavailable. Available: {', '.join(sorted(assets)) or '(none)'}")
    asset = assets[variant]
    archive = download(asset["browser_download_url"])
    with tempfile.TemporaryDirectory(dir=HOME) as tmp:
        root = Path(tmp)
        with zipfile.ZipFile(io.BytesIO(archive)) as z:
            for member in z.infolist():
                if member.is_dir():
                    continue
                if Path(member.filename).name != member.filename or "\\" in member.filename:
                    raise ValueError("Unexpected path in firmware archive")
                (root / member.filename).write_bytes(z.read(member))
        manifest = validate_bundle(root)
        if manifest.get("variant") != variant:
            raise ValueError("Downloaded bundle configuration does not match selection")
        # Unique directories and an atomic pointer keep an interrupted update harmless.
        destination = Path(tempfile.mkdtemp(prefix="build-", dir=HOME))
        shutil.copytree(root, destination, dirs_exist_ok=True)
        atomic_json(HOME / "current.json", {"directory": destination.name})
    print(f"Downloaded {manifest['commit'][:12]} ({variant}) from release {release['tag_name']}")


def current():
    pointer = HOME / "current.json"
    if not pointer.exists():
        raise ValueError("No downloaded firmware. Run 'fw update' first.")
    root = HOME / json.loads(pointer.read_text())["directory"]
    return root, validate_bundle(root)


def locks(port):
    key = hashlib.sha256(port.encode()).hexdigest()[:24]
    return (FileLock(str(HOME / f"{key}.flash.lock")),
            FileLock(str(HOME / f"{key}.serial.lock")))


def flash(cfg):
    root, manifest = current()
    if manifest.get("variant") != require_variant(cfg):
        raise ValueError("Cached firmware is for another configuration. Run 'fw update' or 'fw go' first.")
    port = require_port(cfg)
    intent, access = locks(port)
    print("Waiting for serial port...", flush=True)
    with intent.acquire(timeout=60), access.acquire(timeout=60):
        if is_usb_path(port):
            if not is_termux():
                raise ValueError("Raw USB paths are supported in Termux only; use a serial TTY on desktop Linux")
            run_usb_task("flash", cfg, HOME)
        else:
            subprocess.run([sys.executable, "-m", "esptool"] + flash_arguments(cfg, root, manifest), check=True)
    if cfg.get("manual_reset"):
        print("Flashing complete. Reset the board into normal boot mode.")


def flash_arguments(cfg, root, manifest):
    command = ["--chip", manifest["chip"], "--port", cfg["port"], "--baud", "115200"]
    if cfg.get("manual_reset"):
        command += ["--before", "no-reset", "--after", "no-reset"]
    command += ["write-flash"]
    for key in ("flash_mode", "flash_freq", "flash_size"):
        command += ["--" + key.replace("_", "-"), manifest["flash_settings"][key]]
    for entry in manifest["files"]:
        command += [entry["offset"], str(root / entry["file"])]
    return command


def require_port(cfg):
    if not cfg.get("port"):
        raise ValueError("Set a port first: fw port COM5 (or /dev/ttyACM0 or a serial URL)")
    return cfg["port"]


def is_termux():
    return sys.platform == "android" or bool(os.environ.get("TERMUX_VERSION")) or "/com.termux/" in sys.prefix


def serial_ports():
    # pySerial 3.5 cannot import its port enumerator on Python's Android platform.
    # Keep desktop discovery out of startup and the Android USB path.
    from serial.tools import list_ports
    return list_ports.comports()


def port_hint(port):
    text = " ".join(str(getattr(port, field, "") or "")
                    for field in ("description", "manufacturer", "product")).lower()
    if port.vid == 0x303A or "espressif" in text or re.search(r"\besp(?:32|8266)", text):
        return "Likely ESP (USB identity)"
    if port.vid in (0x10C4, 0x1A86, 0x0403) or any(
            name in text for name in ("cp210", "ch340", "ch341", "ch910", "ft232")):
        return "Possible ESP (USB-UART bridge; not confirmed)"
    return "Other/unknown serial device"


def show_ports(cfg):
    print(f"Saved port: {cfg.get('port') or '(none)'}")
    if is_termux():
        show_termux_usb()
        print("Save a USB path, serial port, or bridge URL with: fw port PORT")
        return
    ports = sorted(serial_ports())
    if not ports:
        print("No serial ports detected.")
    for port in ports:
        selected = " [selected]" if port.device == cfg.get("port") else ""
        print(f"{port.device}{selected} — {port_hint(port)}")
        details = [port.description or "No description"]
        if port.vid is not None and port.pid is not None:
            details.append(f"USB {port.vid:04X}:{port.pid:04X}")
        if port.serial_number:
            details.append(f"serial {port.serial_number}")
        print("  " + " | ".join(details))
    if ports:
        print("USB identity is a hint, not proof of an ESP. No ports were opened or reset.")
    print("Save a serial port or bridge URL with: fw port PORT")


def show_termux_usb():
    executable = shutil.which("termux-usb")
    if not executable:
        print("Android USB listing needs the termux-api package and Termux:API companion app.")
    else:
        try:
            result = subprocess.run([executable, "-l"], capture_output=True, text=True,
                                    timeout=10, check=True)
            devices = json.loads(result.stdout)
            if not isinstance(devices, list) or not all(isinstance(d, str) for d in devices):
                raise ValueError("unexpected device list")
            print("Android USB devices (ESP identity not determined):")
            for device in devices:
                print(f"  {device}")
            if not devices:
                print("  None detected; check OTG connection.")
        except (OSError, ValueError, subprocess.SubprocessError) as exc:
            print(f"Android USB listing unavailable: {exc}")
    print("Select an Android USB device with: fw port /dev/bus/usb/BBB/DDD. Flash/monitor will request USB permission.")


def monitor(cfg):
    port = require_port(cfg)
    key = hashlib.sha256(port.encode()).hexdigest()[:24]
    owner = FileLock(str(HOME / f"{key}.monitor.lock"))
    try:
        owner.acquire(timeout=0)
    except Timeout:
        print("An fw monitor is already running for this port; it will reconnect automatically.")
        return
    try:
        if is_usb_path(port):
            if not is_termux():
                raise ValueError("Raw USB paths are supported in Termux only")
            run_usb_task("monitor", cfg, HOME)
        else:
            _monitor(cfg)
    finally:
        owner.release()


def _monitor(cfg, opener=None, reconnect=True):
    port = require_port(cfg)
    intent, access = locks(port)
    opener = opener or serial.serial_for_url
    print(f"Monitoring {port} at {cfg['baud']} baud. Ctrl+C exits.", flush=True)
    while True:
        try:
            # Claim serial access while holding intent to avoid racing a flasher.
            with intent.acquire(timeout=0):
                access.acquire(timeout=0)
            try:
                with opener(port, baudrate=cfg["baud"], timeout=0.2) as stream:
                    while True:
                        try:
                            intent.acquire(timeout=0)
                            intent.release()
                        except Timeout:
                            print("\nPausing monitor for flashing...", flush=True)
                            break
                        data = stream.read(4096)
                        if data:
                            sys.stdout.buffer.write(data)
                            sys.stdout.buffer.flush()
            finally:
                access.release()
        except Timeout:
            pass
        except (serial.SerialException, OSError) as exc:
            if not reconnect:
                raise
            print(f"\nSerial disconnected: {exc}; retrying...", file=sys.stderr)
            time.sleep(1)
        time.sleep(0.2)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="Override saved port or serial URL")
    sub = parser.add_subparsers(dest="command", required=True)
    for name in ("update", "flash", "mon", "fla", "go", "configs"):
        sub.add_parser(name)
    p = sub.add_parser("port")
    p.add_argument("value", nargs="?", help="Save a port; omit to list ports and likely ESP devices")
    p = sub.add_parser("config")
    p.add_argument("variant", nargs="?", help="Save a config name, e.g. a or b")
    p.add_argument("--repo")
    p.add_argument("--baud", type=int)
    p.add_argument("--manual-reset", action=argparse.BooleanOptionalAction, default=None)
    args = parser.parse_args()
    HOME.mkdir(parents=True, exist_ok=True)
    try:
        cfg = config()
        if args.command == "port" and args.value is None:
            show_ports(cfg)
            return
        if args.command in ("config", "port"):
            if getattr(args, "variant", None) is not None:
                cfg["variant"] = args.variant
                require_variant(cfg)
            for key in ("repo", "baud", "manual_reset"):
                if getattr(args, key, None) is not None:
                    cfg[key] = getattr(args, key)
            if args.command == "port":
                cfg["port"] = args.value
            atomic_json(HOME / "config.json", cfg)
            print(json.dumps(cfg, indent=2))
            return
        if args.command == "configs":
            list_variants(cfg)
            return
        if args.port:
            cfg["port"] = args.port
        if args.command in ("update", "go"):
            update(cfg)
        if args.command in ("flash", "fla", "go"):
            flash(cfg)
        if args.command in ("mon", "fla", "go"):
            monitor(cfg)
    except KeyboardInterrupt:
        return
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError, Timeout, zipfile.BadZipFile) as exc:
        parser.exit(1, f"fw: {exc}\n")


if __name__ == "__main__":
    main()
