#!/usr/bin/env python3
"""Remove fw's managed environment, PATH setup, and (by default) cached data."""
import argparse
import json
import os
from pathlib import Path
import shutil
import sys


def remove_path(prefix, state):
    directory = prefix / "venv" / ("Scripts" if os.name == "nt" else "bin")
    if os.name == "nt":
        import winreg
        with winreg.CreateKey(winreg.HKEY_CURRENT_USER, "Environment") as key:
            try:
                old, kind = winreg.QueryValueEx(key, "Path")
            except FileNotFoundError:
                return
            entries = [entry for entry in old.split(";")
                       if entry.rstrip("\\").lower() != str(directory).lower()]
            winreg.SetValueEx(key, "Path", 0, kind, ";".join(entries))
        import ctypes
        result = ctypes.c_size_t()
        ctypes.windll.user32.SendMessageTimeoutW(65535, 0x1A, 0, "Environment", 2, 5000, ctypes.byref(result))
    else:
        import shlex
        envfile = Path.home() / ".config" / "eyecandy-fw" / "env"
        expected = f'export PATH={shlex.quote(str(directory))}:"$PATH"\n'
        # Another installation may now own this shared PATH setup.
        if envfile.exists() and envfile.read_text() == expected:
            line = f'. {shlex.quote(str(envfile))} # eyecandy-fw installer'
            for name in (".profile", ".bashrc", ".zshrc"):
                profile = Path.home() / name
                if profile.exists():
                    text = profile.read_text()
                    profile.write_text("".join(part for part in text.splitlines(keepends=True)
                                               if part.rstrip("\r\n") != line))
            envfile.unlink()
            try:
                envfile.parent.rmdir()
            except OSError:
                pass


def remove_data(directory):
    if not directory.exists():
        return
    # Remove only files matching fw's own format; preserve unrelated contents.
    for entry in directory.iterdir():
        if entry.is_symlink():
            continue
        if entry.is_file() and (entry.name in ("config.json", "current.json") or
                                entry.name.endswith((".flash.lock", ".serial.lock", ".monitor.lock"))):
            entry.unlink()
        elif entry.is_dir() and entry.name.startswith("build-") and (entry / "manifest.json").is_file():
            shutil.rmtree(entry)
    try:
        directory.rmdir()
    except OSError:
        print(f"Preserved other files in {directory}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prefix", type=Path, default=Path.home() / ".local" / "share" / "eyecandy-fw")
    parser.add_argument("--keep-data", action="store_true", help="Keep saved settings and downloaded firmware")
    args = parser.parse_args()
    prefix = args.prefix.expanduser().resolve()
    marker = prefix / "install-state.json"
    if not marker.exists():
        raise ValueError(f"No managed fw installation at {prefix}; nothing removed")
    state = json.loads(marker.read_text())
    if state.get("product") != "eyecandy-fw":
        raise ValueError("Unrecognized installation marker; nothing removed")
    env = prefix / "venv"
    if os.name == "nt" and Path(sys.prefix).resolve() == env.resolve():
        raise ValueError("Run this uninstaller with system Python (py -3), not fw's venv Python")
    print("Close all fw monitors before uninstalling.")
    # Remove the environment first so a Windows file-in-use failure retains the marker.
    if env.exists():
        shutil.rmtree(env)
    if state.get("path_setup"):
        remove_path(prefix, state)
    if not args.keep_data:
        remove_data(Path(state["data_directory"]))
    for name in ("fw-uninstall.py", "fw-uninstall.sh", "fw-uninstall.bat", "install-state.json"):
        path = prefix / name
        if path.exists():
            path.unlink()
    try:
        prefix.rmdir()
    except OSError:
        pass
    print("fw uninstalled. System Python and system packages were kept. Open a new terminal.")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError) as exc:
        sys.exit(f"fw-uninstall: {exc}")
