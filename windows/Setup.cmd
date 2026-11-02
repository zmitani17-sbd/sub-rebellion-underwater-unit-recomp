@echo off
rem Unpacks your Sub Rebellion (USA) disc image into the "game" folder next to this script.
rem Uses the copy in PCSX2's roms folder by default; you can also drag a .bin or .iso onto this file.
rem The disc image itself is only read, never changed.
setlocal
cd /d "%~dp0"
set "DISC=%~1"
if "%DISC%"=="" set "DISC=%USERPROFILE%\Documents\PCSX2\roms\Sub Rebellion (USA).bin"
if exist "%DISC%" goto unpack
echo Couldn't find the disc image at:
echo   "%DISC%"
echo Drag your Sub Rebellion (USA) .bin or .iso onto Setup.cmd instead.
pause
exit /b 1

:unpack
echo Unpacking "%DISC%" ...
"%~dp0extract_disc.exe" "%DISC%" "%~dp0game" > "%~dp0setup-log.txt" 2>&1
if errorlevel 1 goto failed
if not exist "%~dp0game\SLUS_205.48" goto wronggame
echo.
echo Done. Double-click "Play Sub Rebellion.cmd" to start.
pause
exit /b 0

:failed
type "%~dp0setup-log.txt"
echo.
echo Unpacking failed (details above and in setup-log.txt).
pause
exit /b 1

:wronggame
echo That disc isn't Sub Rebellion USA (SLUS_205.48 not found). This build only runs that version.
pause
exit /b 1
