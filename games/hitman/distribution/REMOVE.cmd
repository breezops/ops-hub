@echo off
call "%~dp0INSTALL.cmd" -Mode Remove %*
exit /b %errorlevel%
