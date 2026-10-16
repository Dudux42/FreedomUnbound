@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\playtest-native-60.ps1" -Reference %*
if errorlevel 1 pause
