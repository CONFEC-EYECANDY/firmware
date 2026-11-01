#!/usr/bin/env python3
"""Install fw into a private venv and add its command to the user's PATH."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
from urllib.request import Request, urlopen
import venv


def fetch(url):
    if not url.startswith("https://"):
        raise ValueError("Installer downloads require HTTPS")
    request = Request(url, headers={"User-Agent": "eyecandy-fw-installer"})
    with urlopen(request, timeout=60) as response:
        return response.read()


def release_info(repo):
    if not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", repo):
        raise ValueError("Repository must be OWNER/REPO")
    release = json.loads(fetch(f"https://api.github.com/repos/{repo}/releases/latest"))
    return release


def wheel_asset(repo):
    return next(a for a in release_info(repo)["assets"]
                if a["name"].startswith("eyecandy_fw_cli-") and a["name"].endswith("-py3-none-any.whl"))


def add_path(directory):
    if os.name == "nt":
        import winreg
        with winreg.CreateKey(winreg.HKEY_CURRENT_USER, "Environment") as key:
            try:
                old, kind = winreg.QueryValueEx(key, "Path")
            except FileNotFoundError:
                old, kind = "", winreg.REG_EXPAND_SZ
            if str(directory).lower() not in [p.rstrip("\\").lower() for p in old.split(";")]:
                winreg.SetValueEx(key, "Path", 0, kind, str(directory) + ";" + old)
        import ctypes
        result = ctypes.c_size_t()
        ctypes.windll.user32.SendMessageTimeoutW(65535, 0x1A, 0, "Environment", 2, 5000, ctypes.byref(result))
    else:
        # A separate env file keeps shell quoting and future installer runs simple.
        envfile = Path.home() / ".config" / "eyecandy-fw" / "env"
        envfile.parent.mkdir(parents=True, exist_ok=True)
        import shlex
        envfile.write_text(f'export PATH={shlex.quote(str(directory))}:"$PATH"\n')
        line = f'. {shlex.quote(str(envfile))} # eyecandy-fw installer'
        profiles = [Path.home() / ".profile", Path.home() / ".bashrc", Path.home() / ".zshrc"]
        for profile in profiles:
            existing = profile.read_text() if profile.exists() else ""
            if line not in existing.splitlines():
                with profile.open("a") as stream:
                    stream.write("\n" + line + "\n")
        print(f"For this terminal: . {shlex.quote(str(envfile))}")


def main():
    if sys.version_info < (3, 10):
        raise ValueError("Install Python 3.10+ first")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", default=os.environ.get("FW_REPO", "TL101GRP/firmware"))
    parser.add_argument("--prefix", type=Path, default=Path.home() / ".local" / "share" / "eyecandy-fw")
    parser.add_argument("--wheel", type=Path, help="Install a local wheel instead of downloading (offline with --no-deps)")
    parser.add_argument("--no-path", action="store_true", help="Do not modify shell profiles or Windows PATH")
    parser.add_argument("--no-deps", action="store_true", help="Use already installed dependencies")
    args = parser.parse_args()
    prefix = args.prefix.expanduser().resolve()
    env = prefix / "venv"
    termux = bool(os.environ.get("TERMUX_VERSION")) or "/com.termux/" in sys.prefix
    marker = prefix / "install-state.json"
    if env.exists() and not marker.exists():
        raise ValueError(f"Refusing to adopt an unmanaged environment: {env}")
    if marker.exists() and json.loads(marker.read_text()).get("product") != "eyecandy-fw":
        raise ValueError("Unrecognized installation marker")
    print(f"Installing fw into {env}", flush=True)
    prefix.mkdir(parents=True, exist_ok=True)
    old_state = json.loads(marker.read_text()) if marker.exists() else {}
    state = {"product": "eyecandy-fw", "path_setup": old_state.get("path_setup", False),
             "data_directory": str(Path(os.environ.get("FW_HOME", Path.home() / ".eyecandy-fw")).expanduser().resolve())}
    marker.write_text(json.dumps(state, indent=2))
    venv.EnvBuilder(with_pip=True, system_site_packages=termux).create(env)
    python = env / ("Scripts/python.exe" if os.name == "nt" else "bin/python")
    command = env / ("Scripts/fw.exe" if os.name == "nt" else "bin/fw")
    with tempfile.TemporaryDirectory() as tmp:
        uninstall_names = ("fw-uninstall.py", "fw-uninstall.sh", "fw-uninstall.bat")
        if args.wheel:
            wheel = args.wheel.resolve()
            for name in uninstall_names:
                source = Path(__file__).resolve().parent / name
                if source.exists():
                    (prefix / name).write_bytes(source.read_bytes())
        else:
            release = release_info(args.repo)
            for name in uninstall_names:
                item = next(a for a in release["assets"] if a["name"] == name)
                (prefix / name).write_bytes(fetch(item["browser_download_url"]))
            asset = next(a for a in release["assets"]
                         if a["name"].startswith("eyecandy_fw_cli-") and a["name"].endswith("-py3-none-any.whl"))
            name = asset["name"]
            if Path(name).name != name or "\\" in name:
                raise ValueError("Invalid wheel filename")
            wheel = Path(tmp) / name
            wheel.write_bytes(fetch(asset["browser_download_url"]))
        # First ensure dependencies, then replace the CLI even if its version is unchanged.
        if not args.no_deps:
            subprocess.run([str(python), "-m", "pip", "install", "--upgrade", str(wheel) + ("[android]" if termux else "")], check=True)
        subprocess.run([str(python), "-m", "pip", "install", "--force-reinstall", "--no-deps", str(wheel)], check=True)
    subprocess.run([str(command), "config", "--repo", args.repo], check=True)
    if not args.no_path:
        add_path(command.parent)
        state["path_setup"] = True
        marker.write_text(json.dumps(state, indent=2))
    print(f"Offline uninstaller saved in: {prefix}")
    print(f"Installed: {command}\nOpen a new terminal, then run: fw port YOUR_PORT\nThen: fw configs, fw config a, fw go")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, StopIteration, subprocess.CalledProcessError) as exc:
        sys.exit(f"fw-install: {exc}\nCheck Python venv support and network access. On Termux install python-cryptography, libusb, and termux-api; additional build dependencies may be needed.")
