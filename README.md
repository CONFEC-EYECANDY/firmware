# Eyecandy firmware

Push to `main` → GitHub builds → a public Release is published → run `fw go`.
The client uses Python 3.10+, **no GitHub CLI, login, ESP-IDF, or project checkout**.
The installer creates its own Python environment and puts `fw` on PATH.

## First push / GitHub setup

The repository must be **public**, with GitHub Actions enabled and repository
policy allowing the workflow's `contents: write` permission to create Releases.
The workflow builds with ESP-IDF pinned to
`4d59230ddff16327812782151ef0afef202dc6d7`, tests the CLI on Linux/Windows, and
publishes a new Release for successful builds of `main` (push or manual run).
Other branches build and upload Actions artifacts but do not publish Releases.

Each release contains `firmware-esp32c3-a.zip`, `firmware-esp32c3-b.zip`, etc.,
the Python CLI wheel, and the installer/uninstaller files. Uploads happen to a draft; only the complete release becomes
**Latest**. Failed builds leave the previous release available. Public Release
assets do not require GitHub authentication and are not subject to Actions
artifact retention. The CI runner uses its built-in `gh` and `GITHUB_TOKEN` to
publish; client devices do not use either.

Wait for the **Firmware** workflow to finish before installing for the first time.
This implementation does not change repository visibility or enable Actions for you.

## Linux: install once

Install Python 3.10+ and venv support using your distribution's package manager.
On Debian/Ubuntu:

```sh
sudo apt install python3 python3-venv curl
curl -fL https://github.com/TL101GRP/firmware/releases/latest/download/fw-install.sh -o fw-install.sh
sh fw-install.sh
```

Open a new terminal (or run the source command printed by the installer), then:

```sh
fw port /dev/ttyACM0
fw config a
fw go
```

Use the actual board port, possibly `/dev/ttyUSB0`. A stable
`/dev/serial/by-id/...` path is preferable. Your user needs access to the port.
On distributions using the `dialout` group:

```sh
sudo usermod -aG dialout "$USER"
```

Log out/in after changing group membership. Other distributions may use different
serial-device permissions. No sudo is needed to run the installer or `fw`.

## Windows: install once

Install Python 3.10+ from python.org, including the Python launcher.
Download [fw-install.bat](https://github.com/TL101GRP/firmware/releases/latest/download/fw-install.bat)
and run it from Command Prompt:

```bat
fw-install.bat
```

The installer uses Windows PowerShell to download the shared installer, then
Python to install the CLI. It adds the command directory to your user PATH.
Close and reopen your terminal application, then:

```bat
fw port COM5
fw config a
fw go
```

Use the COM port shown in Device Manager. Install the board's USB serial driver
if Windows does not recognize it. Administrator access is not required for the CLI.

## Installer behavior / upgrades

Rerun the installer to update `fw` itself. `fw update` updates **firmware**, not
the CLI. Installation preserves saved port/configuration and cached firmware.
The default Python environment is `~/.local/share/eyecandy-fw/venv`.
On Linux/Termux the installer adds an idempotent PATH entry to `.profile`,
`.bashrc`, and `.zshrc`; on Windows it updates your user PATH. It does not install
Python or system packages automatically.

You can also download `fw-install.py` from the Release and run it directly:

```sh
python fw-install.py
```

Advanced options: `--prefix DIRECTORY`, `--no-path`, `--repo OWNER/REPO`, or
`--wheel /path/to/eyecandy_fw_cli-0.2.0-py3-none-any.whl`. Set `FW_REPO` before
running the shell/batch bootstrap to use another public repository with the same
release format. The latest release must contain the expected assets; unrelated
releases marked Latest are not supported. Anonymous GitHub API rate limits apply.

## Uninstall everything installed by fw

Close all `fw` monitors first. The installer saves an offline uninstaller beside
its environment. On Linux/Termux:

```sh
sh ~/.local/share/eyecandy-fw/fw-uninstall.sh
```

On Windows, in Command Prompt:

```bat
"%USERPROFILE%\.local\share\eyecandy-fw\fw-uninstall.bat"
```

This removes the private environment (including its Python package dependencies),
saved settings, cached firmware builds, installer-managed PATH entries, and the
saved uninstaller files. Unrelated files are preserved. System Python, system
packages, and installers you downloaded elsewhere are left intact. Open a new
terminal afterward. Add `--keep-data` to preserve configuration and firmware.
For a custom install prefix, run the saved script in that prefix, or use
`python fw-uninstall.py --prefix DIRECTORY` with system Python.

## Firmware configurations

`main/config.hpp` remains the original local-development configuration. CI
**never builds that original version**. Put complete alternative headers in:

```text
configs/a.hpp
configs/b.hpp
configs/your-name.hpp
```

The configuration name is the filename without `.hpp`. Names may contain letters,
digits, `_`, and `-`, starting with a letter or digit. `a.hpp` starts with node
name `A`; `b.hpp` copies the original settings with node name `B`. Both retain
`SET_ROOT_NODE false`; edit the headers to define the roles/settings you want.

CI discovers every `configs/*.hpp`, copies each over `main/config.hpp`, builds
into a separate build directory, and packages it under that name. It restores
the original header even if a build fails. No configurations means a build
failure, not a fallback to the original. Every configuration must build before
any new release is published. Add another header and push; no workflow edits
are needed.

On the client device:

```sh
fw configs       # List a, b, ... from the latest release
fw config a      # Remember a; no download or flashing yet
fw go            # Download a, flash a, monitor
fw config b      # Switch the saved choice
fw go            # Download b, flash b, monitor
```

There is no implicit configuration: select one before the first download.
Selection persists across sessions and installer upgrades. `fw flash`/`fw fla`
refuse a cached binary from another configuration until you run `fw update` or
`fw go`. These are build-time firmware variants; changing selection on the host
does not change an already flashed ESP until you flash again.

For a local equivalent of CI, activate ESP-IDF and run
`python tools/build_variants.py`. Ordinary `idf.py build` still uses the original
`main/config.hpp`.

## Daily commands

```sh
fw update     # Download the latest published firmware release
fw flash      # Flash cached firmware and exit
fw mon        # Read serial output; Ctrl+C exits
fw fla        # Flash, then monitor
fw go         # Update, flash, then monitor
fw configs    # List configurations in the latest public release
fw config a   # Save selection a for future downloads/flashes
fw config     # Show saved settings
fw config --repo TL101GRP/firmware
fw config --baud 115200
fw --port COM6 flash  # Override the saved port for one invocation
```

`fw flash`, `fw fla`, and `fw mon` work offline after installation/download.
Close other serial applications before use. To keep a monitor open, run `fw mon`
in one terminal and `fw flash` or `fw go` in another. The monitor releases its
port during flashing and reconnects afterward. With `fw go` or `fw fla`, an existing
monitor is reused: the flashing command exits and the existing monitor reconnects.
Only one monitor can own a port at a time. All invocations must use the same port
spelling and `FW_HOME` directory for coordination. Serial output during flashing
is unavailable, and very early boot messages can be missed during reconnection.
The monitor displays output only; interactive serial input is not implemented.

Flashing uses the offsets/settings generated by ESP-IDF and validates all flash
file checksums first. It does not erase the whole chip. A changed partition
layout can still affect stored data. This firmware bundle is for ESP32-C3 boards
compatible with this project's configuration (currently 2 MB flash).

Configuration and downloaded builds live in `~/.eyecandy-fw`; set `FW_HOME` to
use another directory. Updates switch the current-build pointer only after
validation; older cached build directories are retained.

## Android / Termux

Install a supported Termux distribution. Inside Termux:

```sh
pkg install python python-cryptography curl
curl -fL https://github.com/TL101GRP/firmware/releases/latest/download/fw-install.sh -o fw-install.sh
sh fw-install.sh
```

Open a new terminal or run the source command printed by the installer.
The Termux environment allows its system Python packages, so the prebuilt
`python-cryptography` package can satisfy esptool's dependency. Additional build
packages may be needed if other dependencies have no compatible wheels.
Installation has not been validated on a physical phone.

**Installing the CLI does not provide direct Android USB access.** An unrooted
phone normally cannot expose the board as a TTY that desktop Python can open.
USB flashing requires OTG and an Android serial bridge compatible with your
board. The exact bridge depends on the board's USB interface and phone.

If a compatible Android bridge exposes a **local TCP serial endpoint**, configure
its real address, for example:

```sh
fw port socket://127.0.0.1:8888
fw config a
fw config --manual-reset
fw update
```

Configure the bridge's serial baud rate to 115200. For `fw flash`, manually enter
ROM download mode (typically hold BOOT, tap RESET, release BOOT), then run the
command. After successful flashing, reset into normal boot mode and run `fw mon`.
`fw fla` also starts monitoring automatically, but you still must reset the board.
Raw TCP does not carry reset-line or baud-rate controls; a working transparent
binary bridge is necessary. An RFC2217 bridge can instead be configured using
`rfc2217://...`; automatic reset depends on its implementation and hardware.
Only grant the bridge USB access to the intended board and bind it locally.

This is a supported serial-URL configuration path, **not a claim that every
Android bridge/board combination works**. Physical Android flashing remains to
be tested. On a rooted device with an accessible serial TTY, that TTY can be used
instead. Restore desktop automatic reset with `fw config --no-manual-reset`.

## Developing the CLI

```sh
python -m pip install -e ./tools/fw_cli build
python -m unittest discover -s tools/fw_cli/tests -v
python -m unittest discover -s tools/tests -v
python -m build --wheel --outdir dist/cli tools/fw_cli
```

Tests cover corrupted downloads, preservation of the previous cached build,
manifest-based flash arguments, and monitor release/reconnection during flashing.
Hardware flashing is a separate validation step.
