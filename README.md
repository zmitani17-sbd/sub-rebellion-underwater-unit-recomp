# Sub Rebellion: PS2 static recompilation (work in progress)

Sub Rebellion (USA, `SLUS-20548`, Irem/Racjin, 2002) recompiled from its PS2 executable into
C++ with [PS2Recomp](https://github.com/ran-j/PS2Recomp) and run natively on PC. Started
2026-11-01 as a feasibility check.

**No game data is in this repo.** `game/` (the unpacked disc) and `output/` (C++ generated from
the game's executable) are gitignored and recreated from your own disc.

## Status

The recompiled game boots, loads its nine IOP modules, runs its start-up, enters its main loop
(object update, VIF1 packet build, DMA, vblank-paced flips) and draws its first screen, the
memory-card prompt, with correct text: "There is no data for this title on the memory card (8MB)
(for PlayStation®2) in MEMORY CARD slot 1. Start the game without loading data?". Pressing ✕
answers it, and the game moves on to the Irem logo movie, which stays black: the PS2's MPEG
decoder (IPU) isn't emulated. That is the current blocker.

| Stage | State |
|---|---|
| Recompile (4,408 functions) | Done. 0 unhandled instructions, 0 decode failures, 54 indirect jumps on fallback lookup |
| Boot, IOP modules, threads, vblank | Working, after fixes 1–3 below |
| Main loop, DMA, GS drawing, presentation | Working, after fix 4 |
| Memory card | Working: an empty card gives the normal "no data" prompt |
| Font/text | Working, after fix 5 |
| Pad input | Working (keyboard through raylib; see `tools/run_headless.sh` for the keys) |
| Movies (`0\*.DSI`) | **Blocker.** Logo movie starts, screen stays black. Needs IPU emulation; PS2Recomp has none |
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
5. **VIF1 swallowed VIFcodes in the middle of an image upload** (`ps2_vif1_interpreter.cpp`). The
   game uploads its 4-bit font through a DMA chain with TTE on: each REF tag's upper half carries
   `NOP; DIRECTHL 16384` in front of the next piece of the same GIF IMAGE. The runtime treated
   everything after the first DIRECT as raw pixels while the image was still open, so those 8
   bytes went into the texture and shifted the rest of it by 8 bytes (garbled text). Now VIFcodes
   are decoded as on hardware and the next DIRECT's data continues the open image; a DIRECT cut
   off at the end of a DMA buffer carries over to the next one. The old lenient behaviour (raw
   qwords continue an open image) is kept when no DIRECT follows, so two existing upstream tests
   still pass. New test: `VIF1 image split across two DIRECTHL keeps the VIFcodes between them
   out of the image`.

Also in the patch, off unless set (the lookups are cached, so they cost nothing when unset):
- `PS2X_FRAME_DUMP=<dir>`: every 30th presented frame as raw RGBA, display-register probes, and
  at present 150 a dump of GS memory (`vram.bin`) and EE RAM (`eeram.bin`).
- `PS2X_IDLE_DUMP=1`: guest thread wait states when the scheduler idles, and vblank dispatch.
- `PS2X_SPRITE_LOG=1`: one line per distinct textured sprite setup and per distinct GS transfer.
- `PS2X_SEQ_LOG=1`: GIF submissions (path, size, tag, payload hash) and GS draws/transfers in a
  fixed window, plus the first sizeable IMAGE payload to `image_payload.bin`.

PS2Recomp's test suite (built with `-DNDEBUG`, since an existing thread-affinity assert aborts
it otherwise): 446 passed, 1 failure (`VU0 macro mappings cover all S1/S2 enums`, which can't
find `instructions.h` from the test's working directory). That failure is there without the
patch too.

## Rebuild

Linux (what was used, Ubuntu 24.04 cloud container):

```
tools/setup_linux.sh "/path/to/Sub Rebellion (USA).bin"
KEYS="20:x" tools/run_headless.sh 60 run     # ✕ at 20 s answers the memory-card prompt
python3 tools/frames_to_png.py run/frames
```

The disc image is over the 400 MB per-file transfer limit between the PC and the cloud session;
`tools/split-sub-rebellion.cmd` (double-click on Windows) copies it into two pieces in
`Downloads\sub-rebellion-parts`; rejoin with `cat part0 part1 > subreb.bin`.

Windows: PS2Recomp is developed mainly with MSVC; the same steps should work with CMake and
Visual Studio 2022 but haven't been tried for this game.

## Next

1. **Movies (current blocker).** After ✕ the game runs `game_logo_movie` → `racMovieStart` →
   `racMovieStartCdvd`, whose `MovieProc` thread streams the `.DSI` file and feeds the IPU
   (`setDMAtoIPU` is called once; `IpuHandler` never runs). `playMovie`/`onDrawMovie` are called
   every frame and draw nothing. Two ways forward:
   - Quick: make the game treat each movie as finished, to see the title screen and menus. The
     candidates are `racMovieCheckDEndCdvd` (reads byte 5 of the movie state at `*0x274E34`),
     `getMovieDoneFlag` (`-0x571C($gp)`) and the movie object's state bytes `0xB4`–`0xB7` that
     `playMovie` updates. Best done as a runtime game override so it doesn't force a full rebuild.
   - Proper: IPU emulation (MPEG-2 I/P/B decode plus colour conversion through IPU DMA), or
     decoding the `.DSI` streams on the host.
2. **Sound**: SPU2 voices/ADPCM so `MUS.IRX`, `MODHSYN` and the `XA\*.VSD` streams play.
3. **3D**: run the VU1 microcode through the runtime's VU1 interpreter once a mission loads.

## Debugging notes

- The runtime logs only the first 8 `[SIF module]` loads; module 9 (`MUS.IRX`) loading silently
  is normal.
- gdb works on the runner: break on recompiled functions by name with their address suffix,
  e.g. `break readInit_0x10ad18`. Guest registers are 16 bytes apart in `R5900Context`
  (`*(int*)((char*)$rsi + 16*N)` is `$N` inside a recompiled function).
- `tools/run_headless.sh` uses llvmpipe; Xvfb screenshots stay black, frame dumps don't.
- The memory-card "no data" screen is normal for an empty card; no card fix was needed.
- Text problems on the GS side: dump GS memory (`PS2X_FRAME_DUMP`, `vram.bin`) and decode the
  font page with PS2Recomp's own `GSMem` swizzle helpers (call `GSMem::InitLookupTables()`
  first). The swizzle matched hardware; the bug was upstream of the GS, in VIF1 (fix 5).
