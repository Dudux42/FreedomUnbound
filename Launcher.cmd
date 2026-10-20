@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\playtest-native.ps1" -Launcher
if errorlevel 1 (
    echo.
    echo The launcher could not start. The error is shown above.
    pause
)
endlocal
