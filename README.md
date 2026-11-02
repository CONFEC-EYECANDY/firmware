# Eyecandy firmware

## Install

**Linux** — Python 3.10+, venv support, and curl required.

```sh
curl -fL https://github.com/CONFEC-EYECANDY/firmware/releases/latest/download/fw-install.sh -o fw-install.sh
sh fw-install.sh
```

**Windows (Command Prompt)** — install Python 3.10+ first.

```bat
curl.exe -fL https://github.com/CONFEC-EYECANDY/firmware/releases/latest/download/fw-install.bat -o fw-install.bat
fw-install.bat
```

**Android / Termux** — install Termux and the Termux:API companion app from the same source.

```sh
pkg install python python-cryptography termux-api libusb curl
curl -fL https://github.com/CONFEC-EYECANDY/firmware/releases/latest/download/fw-install.sh -o fw-install.sh
sh fw-install.sh
```

## Basic usage (all platforms)

Reopen your terminal and connect the board (USB-OTG on Android).

```sh
fw port             # List ports
fw port PORT        # Linux: /dev/ttyACM0; Windows: COM5; Android: /dev/bus/usb/001/002
fw configs          # List available configurations
fw config NAME      # Select a name from that list
fw go               # Download, flash, and monitor
fw update           # Download only; fw flash flashes the downloaded build
fw mon              # Monitor only; Ctrl+C exits
```

Replace `PORT` with your board's port and `NAME` with a listed configuration.
Allow USB access when Android prompts. Linux users need serial-port permissions.
Rerun the installer to upgrade the CLI.

## Uninstall

Close monitors first. **Linux / Android:**

```sh
sh ~/.local/share/eyecandy-fw/fw-uninstall.sh
```

**Windows (Command Prompt):**

```bat
"%USERPROFILE%\.local\share\eyecandy-fw\fw-uninstall.bat"
```

Removes the CLI, settings, and cached firmware. Reopen your terminal afterward.
