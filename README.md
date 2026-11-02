# Sub Rebellion: PS2 static recompilation (work in progress)

Sub Rebellion (USA, `SLUS-20548`, Irem/Racjin, 2002) recompiled from its PS2 executable into
C++ with [PS2Recomp](https://github.com/ran-j/PS2Recomp) and run natively on PC. Started
2026-11-01 as a feasibility check.

**No game data is in this repo.** `game/` (the unpacked disc) and `output/` (C++ generated from
the game's executable) are gitignored and recreated from your own disc.

## Status

The recompiled game boots, loads its nine IOP modules, runs its start-up, enters its main loop
(object update, VIF1 packet build, DMA, vblank-paced flips) and draws its first screen: the
memory-card warning. The text on that screen is garbled. Nothing past it has been reached.

| Stage | State |
|---|---|
| Recompile (4,408 functions) | Done. 0 unhandled instructions, 0 decode failures, 54 indirect jumps on fallback lookup |
| Boot, IOP modules, threads, vblank | Working, after fixes 1–3 below |
| Main loop, DMA, GS drawing, presentation | Working, after fix 4 |
| Memory card | Game says the card is full (see Next) |
| Font/text | Garbled glyphs |
| Movies (`0\*.DSI`) | Not reached. Needs IPU emulation; PS2Recomp has none |
| Sound (`MUS.IRX`, `XA\*.VSD`) | Not reached. SPU2 isn't emulated (register stubs only) |
| 3D (VU1 microcode) | Not reached |

## The game

- Executable `SLUS_205.48`, 1.78 MB, **not stripped**: 8,637 symbols with the original function
  names (`mainProc`, `racScSleep`, `XaInit`, `COBJ_Draw`...). Use `tools/sym.py` to map addresses.
- Raw Mode 2 CD image, 572,171,040 bytes (2352-byte sectors). `tools/iso9660.py` unpacks it.
- Disc: `MODULES\` (SIO2MAN, PADMAN, MCMAN, MCSERV, LIBSD, MODMIDI, MODHSYN, MODMSIN, MUS, plus
  IOPRP250.IMG), `CDDATA.DIG` (125 MB packed archive), `XA\*.VSD` (PS-ADPCM streams via the custom
  `MUS.IRX`), `0\*.DSI` (Racjin movie container, decoded with the IPU: `setDMAtoIPU`, `IpuHandler`).
- VU1 microcode: `.vutext` 15,872 bytes (1,984 instruction pairs) plus 8 DVP overlays (~15 KB).
- Racjin's frame scheduler: `schedProc` waits on a semaphore signalled by the vblank handler
  `intcVBlankSProc`, then wakes each client registered with `racScAddClient`; threads sleep with
  `racScSleep`.

## Fixes to PS2Recomp (`patches/ps2recomp-sub-rebellion-fixes.patch`)

Applies to PS2Recomp commit `e563698`. Each fix was needed to get further:

1. **`EnableIntc`/`DisableIntc` return value** (`EeScheduler::setIrqCauseEnabled`). The kernel
   returns 1 when the call changed the state; the runtime always returned 0. `racScAddClient` does
   `was = DisableIntc(VBLANK); ...; if (was) EnableIntc(VBLANK)`, so vblank stayed off for good.
   One upstream test (`ps2_runtime_interrupt_tests.cpp`) asserted the old value; updated.
2. **`ReferSemaStatus` returns the semaphore ID** (`Syscalls/Sync.cpp`), not 0. `schedProc`
   signals a client only if `ReferSemaStatus(id, &s) == id`, so no thread ever woke.
3. **Doubled ISO version suffix**. The game opens `cdrom0:\\XA\AQUATITL.VSD;1;1` (its format string
   is `cdrom0:\%s;1` and the names already end in `;1`). Real hardware stops at the first `;`.
   Fixed in `ps2xIOP/src/ps2_path.cpp`, `Syscalls/Helpers/Path.h` and `Stubs/Helpers/Support.h`.
   Without it `XaInit` retried forever.
4. **`sceGsSetDefDispEnv` stand-in left `PMODE`/`SMODE2`/`BGCOLOR` zero** (`writeGsDispEnv` in
   `Stubs/Helpers/Support.h`). The game's own `sceGsPutDispEnv` then wrote `PMODE=0`, both display
   circuits stayed off, and no frame was ever presented.

Also in the patch, off unless set: `PS2X_IDLE_DUMP=1` (guest thread wait states when the
scheduler idles, and vblank handler dispatch), `PS2X_FRAME_DUMP=<dir>` (raw frame dumps plus
display-register probes).

PS2Recomp's test suite (built with `-DNDEBUG`, since an existing thread-affinity assert aborts
it otherwise): 446 passed, 1 failure (`VU0 macro mappings cover all S1/S2 enums`), identical
before and after the patch.

## Rebuild

Linux (what was used, Ubuntu 24.04 cloud container):

```
tools/setup_linux.sh "/path/to/Sub Rebellion (USA).bin"
tools/run_headless.sh 60 run
python3 tools/frames_to_png.py run/frames
```

The disc image is over the 400 MB per-file transfer limit between the PC and the cloud session;
`tools/split-sub-rebellion.cmd` (double-click on Windows) copies it into two pieces in
`Downloads\sub-rebellion-parts`; rejoin with `cat part0 part1 > subreb.bin`.

Windows: PS2Recomp is developed mainly with MSVC; the same steps should work with CMake and
Visual Studio 2022 but haven't been tried for this game.

## Next

1. **Memory card full.** The stand-in answers `GetInfo` with `type=2 free=8192 format=1` and
   `GetDir '/BASLUS-20548/BASLUS-20548'` with -4; the game then shows "not enough free space".
   Find the check in the game's `Mc_*`/card code (symbols are available) and what it expects.
2. **Garbled text.** Likely 4-bit/8-bit CLUT texture sampling or texture upload layout in
   `GSCpuBackend` (`SampleTexture`, `LookupCLUT`, transfers). Compare the font texture in GS memory.
3. **Input** to get past the screen (PS2Recomp reads pads via raylib).
4. **Movies**: IPU emulation for the `.DSI` streams (or decode them on the host).
5. **Sound**: SPU2 voices/ADPCM so `MUS.IRX`, `MODHSYN` and the `XA\*.VSD` streams play.
6. **3D**: run the VU1 microcode through the runtime's VU1 interpreter once a mission loads.

## Debugging notes

- The runtime logs only the first 8 `[SIF module]` loads; module 9 (`MUS.IRX`) loading silently
  is normal.
- gdb works on the runner: break on recompiled functions by name with their address suffix,
  e.g. `break readInit_0x10ad18`. Guest registers are 16 bytes apart in `R5900Context`
  (`*(int*)((char*)$rsi + 16*N)` is `$N` inside a recompiled function).
- `tools/run_headless.sh` uses llvmpipe; Xvfb screenshots stay black, frame dumps don't.
