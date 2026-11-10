@echo off
setlocal
title HITMAN Co-op - movement preview setup
set "PSModulePath=%ProgramFiles%\WindowsPowerShell\Modules;%SystemRoot%\System32\WindowsPowerShell\v1.0\Modules"
echo.
echo   HITMAN CO-OP - FOUR-PLAYER MOVEMENT PREVIEW
echo   Close HITMAN and co-op launchers before changes.
echo.
echo   1. Install or update movement preview
echo   2. Restore the verified Foundation 4.1
echo   3. Remove co-op
echo   4. Open the current co-op launcher
echo   5. Exit
echo.
choice /c 12345 /n /m "Choose 1, 2, 3, 4 or 5: "
set "menu_choice=%errorlevel%"
if "%menu_choice%"=="1" goto install
if "%menu_choice%"=="2" goto restore
if "%menu_choice%"=="3" goto remove
if "%menu_choice%"=="4" goto launch
exit /b 0
:install
call "%~dp0INSTALL.cmd" -Mode Auto
exit /b %errorlevel%
:remove
call "%~dp0INSTALL.cmd" -Mode Remove
exit /b %errorlevel%
:restore
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "%~dp0Restore-4.1.ps1"
set "restore_result=%errorlevel%"
pause
exit /b %restore_result%
:launch
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "%~dp0Open-Launcher.ps1"
exit /b %errorlevel%
