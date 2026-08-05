===============================================================================
 PicoDrive 32X - SH2 dynarec, PowerPC backend for Xbox 360
 Build and debugging guide (Salvia / VS2010 + XDK)
===============================================================================

SUMMARY
-------
The SH2 dynarec's PPC backend (cpu/drc/emit_ppc.c) was written upstream for
ppc64le (POWER, 64-bit little-endian). Xenon (Xbox 360) is 32-bit BIG-ENDIAN
PowerPC. emit_ppc.c carries the validated Xenon adaptations:
  - 64-bit ops disabled (#if 0 /* Xbox 360 is 32-bit */)
  - GCC builtins -> MSVC intrinsics (_BitScanForward, etc.)
  - LR saved INSIDE the frame in emith_sh2_drc_entry/exit (critical fix)
  - correct SMC flush for Xenon (dcbst/sync/icbi/sync/isync)
  - emith_move_r_imm_s8_patchable widened to s16 (RTS return prediction)
  - mtctr/bctr fallback for jumps out of the +-32MB range
  - emith_pass_arg_r/imm fix (host_arg2reg), used by the DRC_CMP path

STATUS: THE PPC DRC WORKS. 32X games boot and play at full speed with the full
DRC (master+slave), with LOOP_DETECTION/LOOP_OPTIMIZER turned off. Those two
features (idle-skip + pinned loops) still cause a hang on the PPC port and are
OFF BY DEFAULT on Xbox 360 (compiler.c); they remain a pending performance
optimization (use SALVIA_ENABLE_LOOP + SALVIA_NO_LOOPDET/SALVIA_NO_LOOPOPT to
debug them). Root-cause fixes resolved along the way: uninitialized structs
from the VS2010 rewrite (rcache/branch_targets/blx/pinned) and push/pop
clobbering the PPC parameter save area (poll stubs).

KNOWN ISSUE - Doom 32X: hangs when firing / picking up an item (the 3D freezes,
Genesis music keeps playing). Ruled out timing (no real cycle drift) and the
data optimizers; it is a codegen bug in the renderer/comm code around
0x0204654a/0x02049284 that DRC_CMP cannot pinpoint because the divergence
cascades. Next step: offline SH2 disassembly of that path from the ROM.


BUILD MODES (Preprocessor Definitions of the Xbox 360 configuration)
-------------------------------------------------------------------
The arch is selected with __ppc__ (+ __BIG_ENDIAN__, _XBOX). The DRC with DRC_SH2.

  (1) NORMAL DRC  (goal: run games with the recompiler)
      ...;DRC_SH2;...
      This is the current default in Debug|Xbox 360 and Release|Xbox 360.

  (2) DRC_CMP - COMPARE  (find the first DRC-vs-interpreter divergence)
      ...;DRC_SH2;DRC_CMP;...
      The DRC runs and, after EACH instruction, do_sh2_cmp() reads the tracelog
      and compares all SH2 registers + SR. The first register/PC/SR divergence
      is logged as "bad rX: <drc> <ref>" plus a register dump, then the
      comparison STOPS (a single clean divergence in the log; the game keeps
      running). Requires a tracelog produced beforehand (RECORD mode).
      NOTE: DRC_CMP disables PROPAGATE_CONSTANTS/LOOP_*/T_/DIV_OPTIMIZER in
      compiler.c to keep exact lockstep with the interpreter.

  (3) DRC_CMP - RECORD  (generate the reference tracelog)
      ...;DRC_CMP;DRC_CMP_RECORD;...
      Forces both SH2s to the interpreter (32x.c); do_sh2_trace() WRITES the
      tracelog. Run the game a bit past the point of interest and exit.

  (4) Slave isolation  (is the bug master- or slave-side?)
      ...;DRC_SH2;SALVIA_FORCE_SLAVE_INTERP;...
      master = DRC, slave = interpreter. If the hang/crash disappears, the bug
      is slave-side.


DRC_CMP GATES / FILTERS (define alongside DRC_CMP)
-------------------------------------------------
  SALVIA_CMP_SH2=0        compare ONLY one SH2 (0=master, 1=slave). Essential on
                          the dual-SH2 32X: the interleaved two-SH2 trace drifts
                          out of sync, so compare a single self-consistent stream.
  SALVIA_CMP_START_PC=0xADDR   don't start comparing until the SH2 first reaches
                          this PC (e.g. a loop entry), so the comparison starts
                          ALIGNED and the first divergence is the exact culprit.
  SALVIA_CMP_START_FRAME=N     don't start until video frame N (skip boot/menu,
                          shrink the tracelog).
  Use the SAME gate values in RECORD and COMPARE so both sides stay aligned.


WORKFLOW TO CATCH A DIVERGENCE
------------------------------
  1. Build in RECORD mode (3) with SALVIA_CMP_SH2=0 (+ a gate if useful). Run the
     failing game a couple of seconds PAST the failure, with NO input so it is
     deterministic. This writes the tracelog. Exit.
  2. Build in COMPARE mode (2) with the SAME SALVIA_CMP_* defines (and usually
     SALVIA_FORCE_SLAVE_INTERP, so the slave matches the RECORD run). Keep the
     tracelog. Run the same game the same way.
  3. In the libretro log, the first "bad ..." line plus "[Salvia] DRC @ pc=..."
     is the register and PC where the DRC diverges from the interpreter - the
     instruction to review in emit_ppc.c / compiler.c.
  Determinism: same console, same ROM, same reset, no input.

  The tracelog is buffered (256KB) and lives at game:\tracelog.bin. In COMPARE
  mode tl_write is a defensive no-op, so it can never truncate the tracelog.


TARGETED OPCODE DUMP
--------------------
  SALVIA_DUMP_PC=0xADDR   with the normal DRC build (no DRC_CMP needed), prints
                          each SH2 instruction within +-0x40 of the address as it
                          is compiled: op, cycles, size, immediate, and the
                          read/written register masks. Handy to disassemble a
                          specific code path (e.g. a loop that hangs).


NOTES
-----
  - game:\ is the .xex directory (writable on the devkit); the current dir may
    be read-only.
  - To go back to the pure interpreter: use __DRC_SH2 (double underscore) in the
    vcxproj, or set the picodrive_drc core option to disabled.
