@echo off
rem Starts the recompiled Sub Rebellion. Keep this window open while playing; everything the
rem game prints goes to log.txt (useful if something breaks).
setlocal
if not exist "%~dp0game\SLUS_205.48" goto nosetup
cd /d "%~dp0game"
rem Window size at start (resizable; F11 toggles borderless fullscreen). The picture keeps 4:3.
if not defined PS2X_WINDOW_SIZE set PS2X_WINDOW_SIZE=1280x960
echo Sub Rebellion is running. Close the game window to quit.
echo Log: "%~dp0log.txt"
"%~dp0SubRebellion.exe" "%~dp0game\SLUS_205.48" > "%~dp0log.txt" 2>&1
if errorlevel 1 goto crashed
exit /b 0

:crashed
echo.
echo The game stopped with an error. The last lines of log.txt:
powershell -NoProfile -Command "Get-Content -Tail 25 '%~dp0log.txt'"
pause
exit /b 1

:nosetup
echo Run Setup.cmd first (it unpacks the game files from your disc image).
pause
exit /b 1
