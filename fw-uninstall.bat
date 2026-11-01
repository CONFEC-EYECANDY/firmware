@echo off
setlocal
if not exist "%~dp0fw-uninstall.py" (
    echo Place fw-uninstall.py beside this script, or use the copy saved in %%USERPROFILE%%\.local\share\eyecandy-fw.
    exit /b 1
)
set "FW_PREFIX="
if exist "%~dp0install-state.json" set FW_PREFIX=--prefix "%~dp0."
where py >nul 2>nul
if %errorlevel% equ 0 (
    py -3 "%~dp0fw-uninstall.py" %FW_PREFIX% %*
    exit /b
) else (
    python "%~dp0fw-uninstall.py" %FW_PREFIX% %*
    exit /b
)
