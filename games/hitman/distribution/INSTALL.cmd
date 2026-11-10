@echo off
setlocal
title HITMAN Co-op - friend setup
if not exist "%~dp0Install-Friend.ps1" (
  echo Extract the entire ZIP first, then run INSTALL.cmd from the extracted folder.
  pause
  exit /b 1
)
rem Keep the install independent of a calling PowerShell 7/custom module path.
set "PSModulePath=%ProgramFiles%\WindowsPowerShell\Modules;%SystemRoot%\System32\WindowsPowerShell\v1.0\Modules"
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-Friend.ps1" %*
set "setup_result=%errorlevel%"
echo.
pause
exit /b %setup_result%
