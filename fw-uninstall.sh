#!/bin/sh
set -eu
fw_dir=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
if command -v python3 >/dev/null 2>&1; then
    fw_python=python3
else
    fw_python=python
fi
# The installer saves this script and fw-uninstall.py together for offline use.
if [ ! -f "$fw_dir/fw-uninstall.py" ]; then
    echo 'Place fw-uninstall.py beside this script, or use the copy saved in ~/.local/share/eyecandy-fw.' >&2
    exit 1
fi
if [ -f "$fw_dir/install-state.json" ]; then
    exec "$fw_python" "$fw_dir/fw-uninstall.py" --prefix "$fw_dir" "$@"
fi
exec "$fw_python" "$fw_dir/fw-uninstall.py" "$@"
