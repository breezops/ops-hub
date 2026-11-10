@echo off
setlocal
title HITMAN / CO-OP - OPS HUB
if not exist "%~dp0updater-config.json" (
  echo Extract the entire ZIP first, then open START-HERE.cmd.
  pause
  exit /b 1
)
start "" "%~dp0HITMAN-Coop-Party-Preview.exe"
