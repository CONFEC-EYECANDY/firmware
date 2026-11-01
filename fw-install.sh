#!/bin/sh
# Bootstrap the Python installer; no GitHub CLI, checkout, or manual venv setup.
set -eu
if command -v python3 >/dev/null 2>&1; then
    fw_python=python3
elif command -v python >/dev/null 2>&1; then
    fw_python=python
else
    echo 'Python 3.10+ is required. Linux: install python3 and python3-venv. Termux: pkg install python python-cryptography termux-api libusb' >&2
    exit 1
fi
"$fw_python" -c 'import sys; sys.exit(0 if sys.version_info >= (3, 10) else "Python 3.10+ is required")'
fw_tmp=$(mktemp -d)
trap 'rm -rf "$fw_tmp"' EXIT HUP INT TERM
fw_repo=${FW_REPO:-TL101GRP/firmware}
"$fw_python" - "$fw_repo" "$fw_tmp/fw-install.py" <<'PY'
import pathlib, re, sys, urllib.request
repo, dest = sys.argv[1:]
if not re.fullmatch(r'[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+', repo):
    sys.exit('FW_REPO must be OWNER/REPO')
url = f'https://github.com/{repo}/releases/latest/download/fw-install.py'
try:
    with urllib.request.urlopen(url, timeout=60) as response:
        pathlib.Path(dest).write_bytes(response.read())
except Exception as exc:
    sys.exit(f'Cannot download installer: {exc}. Wait for the first public release.')
PY
"$fw_python" "$fw_tmp/fw-install.py" --repo "$fw_repo" "$@"
