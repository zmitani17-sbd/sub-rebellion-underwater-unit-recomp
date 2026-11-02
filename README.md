# Sub Rebellion: PS2 static recompilation (work in progress)

Sub Rebellion (USA, `SLUS-20548`, Irem/Racjin, 2002) recompiled from its PS2 executable into
C++ with [PS2Recomp](https://github.com/ran-j/PS2Recomp) and run natively on PC. Started
2026-11-01 as a feasibility check.

**No game data is in this repo.** `game/` (the unpacked disc) and `output/` (C++ generated from
the game's executable) are gitignored and recreated from your own disc.

## Status

The recompiled game boots, runs its front end (memory-card prompt, title screen, NEW GAME,
difficulty select, "NOW LOADING...") and plays the in-engine 3D intro in full detail: sunken
skyscrapers, attack submarines, torpedoes, explosions, bubbles, fog, with its subtitles ("Fifteen
years ago... humankind did not stop fighting amongst themselves."). It runs natively on Windows
(MinGW cross-build, tested on a Ryzen 7 9800X3D at roughly 7-15 fps in menus and 3-5 fps in 3D)
and Linux. The logo movies are skipped (no IPU emulation) and there is no sound yet. Next: see
whether the intro hands over to a playable mission.

| Stage | State |
|---|---|
| Recompile (4,408 functions) | Done. 0 unhandled instructions, 0 decode failures, 54 indirect jumps on fallback lookup |
| Boot, IOP modules, threads, vblank | Working, after fixes 1–3 below |
| Main loop, DMA, GS drawing, presentation | Working, after fix 4 |
| Memory card | Working: an empty card gives the normal "no data" prompt |
| Font/text | Working, after fix 5 |
| Pad input | Working (keyboard through raylib; see `tools/run_headless.sh` for the keys) |
| Front end (title, menus, loading screen) | Working |
| Movies (`0\*.DSI`) | Skipped (`overrides/zz_sub_rebellion_overrides.cpp`). Playing them needs IPU emulation; PS2Recomp has none |
| Sound (`MUS.IRX`, `XA\*.VSD`) | Not reached. SPU2 isn't emulated (register stubs only) |
| 3D (VU1 microcode) | Working in the intro, after fixes 6 and 11 |
| IOP sound driver (`MUS.IRX`) talking back to the EE | Working, after fix 9 |

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

6. **VU1 `JR`/`JALR` read a stale VI register** (`vu/ps2_vu1_lower.cpp`). The interpreter gave
   every branch the old value of a VI register written by the instruction just before it. That
   is right for the conditional branches (IBEQ, IBNE, ...) but not for `JR`/`JALR`: PCSX2's
   microVU applies the rule only to conditional branches and its interpreter's `_vuJR` reads the
   register directly. Racjin's VU1 code starts every routine through a jump table, `B 0x340` with
   `IADDIU vi1, vi0, <routine>` in the delay slot, then `JR vi1` at 0x340. With the stale vi1,
   each call ran the previous routine instead, kicked garbage to the GS ("VU1 reserved lower
   ... 0xfffffffb" was the XGKICK overflow code) and no 3D was drawn. Upstream's test `JR shares
   the one-instruction VI branch visibility rule` asserted the old behaviour; it is replaced by
   `JR uses a VI value written by the instruction just before it`.

7. **Software renderer speed** (`gs/gs_cpu_backend.cpp`). Everything (game code, GS, VU1, IOP)
   ran on one thread, and ~70% of it was the CPU rasterizer. Now: large sprites and triangles
   (2048+ pixels) are split into 8-row bands across a thread pool (hardware threads minus 2, at
   most 8; `PS2X_GS_THREADS=<n>` overrides, 1 turns it off), each worker with its own copy of the
   texture page cache; triangles only test the pixels near each row's span instead of the whole
   bounding box; bilinear filtering rounds inline instead of calling `lroundf` four times per
   texel. `tools/gsparity.cpp` draws 400 random large textured/blended/depth-tested primitives
   and hashes GS memory: the hash is identical with 1, 2, 4 and 8 threads and identical to the
   unmodified upstream renderer, which takes 1.5x as long single-threaded.
8. **`PS2X_DEINTERLACE=weave`** shows whole frames in interlaced field mode instead of alternate
   lines per refresh, which made the picture bounce by a line and halved vertical detail.
   Upstream's line-doubling stays the default (its test expects it); the Sub Rebellion override
   sets weave.

9. **IOP -> EE RPC** (`ps2xIOP/src/emulator/services/iop_rpc.cpp`, `iop_host.h`,
   `Kernel/Syscalls/RPC.cpp`, `ps2_iop_host.cpp`). The IOP's `sceSifBindRpc`/`sceSifCallRpc`
   returned 0 without doing anything, so an IRX could never reach a server the EE registered.
   `MUS.IRX` binds to the game's sid 0x1011 (`cbFunc`, which feeds `musAdpcmStreamCallback`) in a
   retry loop that spun forever from boot, eating IOP time. Bind now looks up the EE server and
   fills `cd->server`; a call copies the sent data into a fresh EE buffer and queues the server
   function like the existing IOP -> EE SIF-command path. Reply buffers and IOP end functions are
   not supported yet (logged once).
10. **VIF1 GS -> EE readback** (`ps2_memory.cpp`). A VIF1 DMA with CHCR.DIR = 0 while GS BUSDIR is
    set (`sceDmaRecvN` after a local->host transfer) was treated as a send: the destination
    buffer went to VIF1 as VIFcodes. It now copies the GS transfer output into EE memory and
    completes the channel.
11. **DMA chain tag limit** (`ps2_memory.cpp`). Chains stopped silently after 4096 tags. Sub
    Rebellion's heavier intro frames are larger (5.6 MB by tag 4096), so most of their 3D was
    never sent (the flat grey scenes), and when a chain stopped on a non-END tag, `graphProc`
    took it for a readback point and waited forever for the FINISH at the frame's end (the hang
    at "all were forced to adopt new forms."). The guard is now 262144 tags or 64 MB, with a
    warning when hit.

Also in the patch, off unless set (the lookups are cached, so they cost nothing when unset):
- `PS2X_FRAME_DUMP=<dir>`: every 30th presented frame as raw RGBA, display-register probes, and
  at present 150 a dump of GS memory (`vram.bin`) and EE RAM (`eeram.bin`).
- `PS2X_IDLE_DUMP=1`: guest thread wait states when the scheduler idles, and vblank dispatch.
- `PS2X_SPRITE_LOG=1`: one line per distinct textured sprite setup and per distinct GS transfer.
- `PS2X_SEQ_LOG=1`: GIF submissions (path, size, tag, payload hash) and GS draws/transfers in a
  fixed window, plus the first sizeable IMAGE payload to `image_payload.bin`.
- `PS2X_VU_LOG=1`: VU1 `MPG` uploads, `MSCAL` starts, every `XGKICK` (address register, value,
  first GIF tag), XGKICK overflows with the packet walked, and GIF packets/bytes per path every
  5 seconds.
- `PS2X_RUNTIME_LIB_COMPILE_OPTIONS` (CMake): build the runtime library at e.g. `-O2`.
- `PS2X_IOP_PC_LOG=1`: every 3 s, where IOP threads are when their timeslices end and the most
  read IOP hardware registers (found the `MUS.IRX` bind loop at 0x5D2F4).
- `PS2X_FINISH_LOG=1`: each VIF1 DMA chain that stops on a non-END tag, with the GS FINISH
  writes since the last one, CSR, and pending VIF1 DIRECT/IMAGE state (found fix 11).

PS2Recomp's test suite (built with `-DNDEBUG`, since an existing thread-affinity assert aborts
it otherwise): 446 passed, 1 failure (`VU0 macro mappings cover all S1/S2 enums`, which can't
find `instructions.h` from the test's working directory). That failure is there without the
patch too. The patch adds one test (fix 5) and replaces one (fix 6).

## Game-specific overrides (`overrides/`)

`zz_sub_rebellion_overrides.cpp` registers a PS2Recomp game override for `SLUS_205.48`. It
sets `PS2X_DEINTERLACE=weave`, makes stdout unbuffered (so a redirected `log.txt` stays
current), and replaces `racMoviePlay` (0x114218) with a stand-in that returns 0 without creating the movie
object, which is what the game does for an unknown movie id; no caller uses the result. The
logo sequence then moves straight on. Set `PS2X_PLAY_MOVIES=1` to turn the skip off.
`tools/setup_linux.sh` copies `overrides/*.cpp` into `ps2xRuntime/src/runner/`; the `zz_` name
keeps it in the last unity-build batch, so editing it rebuilds in about 10 seconds.

## Rebuild

Linux (what was used, Ubuntu 24.04 cloud container):

```
tools/setup_linux.sh "/path/to/Sub Rebellion (USA).bin"
KEYS="20:x 40:Return 50:Return 60:Return 75:x 90:x" tools/run_headless.sh 240 run
    # ✕ answers the card prompt, Enter (Start) at the title, ✕ picks NEW GAME, then NORMAL
python3 tools/frames_to_png.py run/frames
```

The disc image is over the 400 MB per-file transfer limit between the PC and the cloud session;
`tools/split-sub-rebellion.cmd` (double-click on Windows) copies it into two pieces in
`Downloads\sub-rebellion-parts`; rejoin with `cat part0 part1 > subreb.bin`.

Speed: in the 2-core cloud container the game runs at about 3 frames per second, almost all of
it in PS2Recomp's software GS renderer (`GSCpuBackend::DrawSprite`/`WritePixel`, filling
full-screen sprites). The patch adds `PS2X_RUNTIME_LIB_COMPILE_OPTIONS` so the runtime library
can be built at `-O2` while the generated code stays at `-O1`; `setup_linux.sh` uses it. A
desktop CPU will be faster, but a GPU renderer is what would make it full speed.

Windows: `tools/build_windows.sh` cross-compiles a Windows build on Linux with MinGW-w64 (after
`setup_linux.sh`) into `dist/windows/`: a static `SubRebellion.exe` that needs only Windows' own
DLLs, `extract_disc.exe` (C++ port of `tools/iso9660.py`, takes `.bin` or `.iso`), `Setup.cmd`
(unpacks the user's disc image into `game\`), `Play Sub Rebellion.cmd` (logs to `log.txt`) and a
README with the controls (from `windows/`). Tested under Wine 9: boots, takes input, reaches the
title screen. On a Ryzen 7 9800X3D + RX 9070 XT the first build (single-threaded renderer) ran at
1-3 fps and got into the intro ("all were forced to adopt new forms.") after ~10 minutes. An MSVC build should also work but hasn't been tried for this game.

## Next

1. **Into a mission.** The intro now plays through; see what follows it and whether a mission
   loads and takes pad input.
2. **Speed**: the 3D scenes are VU1-interpreter-bound on the game thread (`commitReadyPipelines`,
   `calculatePairReadyCycle` per cycle), plus triangle drawing. Cheaper VU1 pipeline bookkeeping,
   then running GS work alongside the EE, then a GPU renderer.
3. **Sound**: SPU2 voices/ADPCM so `MUS.IRX`, `MODHSYN` and the `XA\*.VSD` streams play. Note
   `MUS.IRX` calls `modhsyn` export 23 (voice count) constantly, which keeps the IOP busy.
4. **Movies**: IPU emulation, or decode the `.DSI` streams on the host. Skipped until then.

## Debugging notes

- The runtime logs only the first 8 `[SIF module]` loads; module 9 (`MUS.IRX`) loading silently
  is normal.
- gdb works on the runner: break on recompiled functions by name with their address suffix,
  e.g. `break readInit_0x10ad18`. Guest registers are 16 bytes apart in `R5900Context`
  (`*(int*)((char*)$rsi + 16*N)` is `$N` inside a recompiled function).
- `tools/run_headless.sh` uses llvmpipe; Xvfb screenshots stay black, frame dumps don't.
- VU1 microcode: `python3 tools/vudis.py --build game/SLUS_205.48 vu1code.bin` rebuilds VU1
  code memory (eight `MPG` uploads stored in `.vutext`, each after a "NOP; MPG" pair), then
  `python3 tools/vudis.py vu1code.bin 340 24` disassembles. Routine entry points are the ELF's
  absolute symbols `racMpgVu1*` (MSCAL immediates: `racMpgVu1MulMtx` = 0x2E → address 0x170);
  each is a jump-table entry that loads the routine's address into vi1 and branches to the
  `JR vi1` dispatcher at 0x340.
- The memory-card "no data" screen is normal for an empty card; no card fix was needed.
- IRX modules: `tools/irx_calls.py <irx> <library> <ordinal>` lists call sites of an imported
  function (IRX `.text` loads at the module base printed in the log, e.g. `MUS.IRX` at 0x5CA00).
- Where the EE is stuck: break on `PS2Runtime::dispatchGuestBranch` in gdb and print `$rcx`
  (target) and `$r8` (source guest PC) for a few hits; the guest PC is at offset 512 of
  `R5900Context`.
- Text problems on the GS side: dump GS memory (`PS2X_FRAME_DUMP`, `vram.bin`) and decode the
  font page with PS2Recomp's own `GSMem` swizzle helpers (call `GSMem::InitLookupTables()`
  first). The swizzle matched hardware; the bug was upstream of the GS, in VIF1 (fix 5).
