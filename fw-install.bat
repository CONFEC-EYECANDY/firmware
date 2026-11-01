@echo off
setlocal
rem Download and run the shared installer using Python; no gh CLI needed.
where py >nul 2>nul
if %errorlevel% equ 0 (
    set "FW_PYTHON=py"
    set "FW_PYTHON_ARGS=-3"
) else (
    where python >nul 2>nul
    if errorlevel 1 (
        echo Install Python 3.10+ from python.org, then run this installer again.
        exit /b 1
    )
    set "FW_PYTHON=python"
    set "FW_PYTHON_ARGS="
)
%FW_PYTHON% %FW_PYTHON_ARGS% -c "import sys; sys.exit(0 if sys.version_info >= (3, 10) else 'Python 3.10+ is required')"
if errorlevel 1 exit /b 1
if not defined FW_REPO set "FW_REPO=TL101GRP/firmware"
set "FW_BOOTSTRAP=%TEMP%\fw-install-%RANDOM%-%RANDOM%.py"
powershell -NoProfile -Command "$ErrorActionPreference='Stop'; if ($env:FW_REPO -notmatch '^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$') { throw 'Invalid FW_REPO' }; [Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -UseBasicParsing -Uri ('https://github.com/' + $env:FW_REPO + '/releases/latest/download/fw-install.py') -OutFile $env:FW_BOOTSTRAP"
if errorlevel 1 (
    echo Cannot download installer. Wait for the first successful public release.
    exit /b 1
)
%FW_PYTHON% %FW_PYTHON_ARGS% "%FW_BOOTSTRAP%" --repo "%FW_REPO%" %*
set "FW_RESULT=%errorlevel%"
del "%FW_BOOTSTRAP%" >nul 2>nul
exit /b %FW_RESULT%
