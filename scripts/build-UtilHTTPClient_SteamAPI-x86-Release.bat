@echo off
setlocal
set "Configuration=Release"
call "%~dp0build-UtilHTTPClient_SteamAPI-x86.bat" %*
exit /b %errorlevel%
