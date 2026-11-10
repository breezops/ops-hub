@echo off
call "%~dp0INSTALL.cmd" -Mode Auto %*
exit /b %errorlevel%
