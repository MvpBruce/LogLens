@echo off
setlocal
powershell -ExecutionPolicy Bypass -File "%~dp0scripts\package_windows.ps1" %*
