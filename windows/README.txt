SUB REBELLION (USA, SLUS-20548) - PC RECOMPILATION, TEST BUILD
================================================================

This is the PS2 game's own code translated to x86-64 and compiled as a Windows program
(PS2Recomp plus our fixes). It is a work in progress. Expect it to be slow and incomplete.

FIRST TIME
  1. Double-click Setup.cmd. It unpacks the game files from
     Documents\PCSX2\roms\Sub Rebellion (USA).bin into the "game" folder here (about 500 MB).
     Elsewhere? Drag your .bin or .iso onto Setup.cmd.
  2. Double-click "Play Sub Rebellion.cmd".
  Windows may say it "protected your PC" because the .exe isn't signed:
  More info > Run anyway.

CONTROLS (keyboard)          XInput controllers also work.
  X / Space ....... Cross        Arrow keys ..... D-pad
  C ............... Circle       W A S D ........ left stick
  Z ............... Square       Enter .......... Start
  V ............... Triangle     Right Shift .... Select
  Q / E ........... L1 / R1      1 / 3 .......... L2 / R2

WHAT TO EXPECT
  - Memory-card prompt: there's no save yet, press X to start without loading.
  - Title: press Enter (Start), then NEW GAME and a difficulty.
  - The logo and story movies are skipped (PS2 video decoding isn't emulated yet).
  - No sound yet.
  - Speed: the graphics are drawn by the CPU, so it runs well below 60 fps.
  - Last tested: the 3D intro plays ("Fifteen years ago..."). Beyond that is untested.
  - The window can be resized or maximized.
  - Saves go to game\mc0.

IF SOMETHING GOES WRONG
  Send log.txt (next to this file) and a screenshot or a short description of where it
  stopped.

Source, patch and notes: github.com/zmitani17-sbd/sub-rebellion-underwater-unit-recomp (private)
