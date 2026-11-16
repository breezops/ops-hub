@echo off
setlocal
title HITMAN Co-op - restore tested 0.1.3
set "PSModulePath=%ProgramFiles%\WindowsPowerShell\Modules;%SystemRoot%\System32\WindowsPowerShell\v1.0\Modules"
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "%~dp0Restore-Tested.ps1" %*
set "restore_result=%errorlevel%"
echo.
pause
exit /b %restore_result%
