@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0run_selftest.ps1" %*
exit /b %ERRORLEVEL%
