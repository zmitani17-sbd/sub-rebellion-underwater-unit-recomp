@echo off
rem Same as "Play Sub Rebellion.cmd" but with drawing on the game thread, as in earlier builds
rem (no separate GS thread). For comparing speed: note the "game N fps" in the window title.
set PS2X_GS_ASYNC=0
call "%~dp0Play Sub Rebellion.cmd"
