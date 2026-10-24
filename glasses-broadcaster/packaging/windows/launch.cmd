@echo off
setlocal
"%~dp0glasses-broadcaster.exe" %*
set "status=%errorlevel%"
if not "%status%"=="0" (
    echo.
    echo Glasses Broadcaster could not start ^(exit %status%^).
    pause
)
exit /b %status%