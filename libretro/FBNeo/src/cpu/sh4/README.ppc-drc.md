# SH3 → PowerPC dynamic recompiler

This backend is enabled for Xbox 360 builds of FBNeo's CV1000 driver, including
`ddpsdoj`. Other host builds keep the existing interpreter. The optional
`SH3_PPC_DRC_TEST` build uses the same emitter on 32-bit big-endian PowerPC Linux.

## Selecting the engine

After starting a CV1000 game, open **Options → Core Options → DIP Switches**:

- **SH3 CPU Engine → Dynamic Recompiler** (Xbox default).
- **Speed Hacks → On** (default): enables the existing batched timer path in
  which compiled blocks execute. With this setting Off, execution uses the
  interpreter's per-instruction timer path.
- **Interpreter** selects the original engine. Switching takes effect at the
  next emulation frame and does not require resetting the game.

Thread Blitter and the existing render-core selection remain independent of
CPU translation. Two render cores can run the SH3 and blitter concurrently.

## Execution and timing

`sh3_drc_ppc.h` emits PowerPC machine code for common integer operations,
loads/stores, comparisons, shifts, carry rotations, multiply and DIV1 operations,
and branches. It caches guest registers in volatile host registers. The emitted
functions use the 32-bit ABI, leave nonvolatile registers untouched, and perform
32-bit comparisons explicitly even though Xenon has 64-bit general registers.

Unsupported instructions, memory-mapped devices, alignment guards, internal
registers and pending interrupts retain the interpreter path. A partial block
commits its register values, PC and consumed cycles before requesting one
interpreted instruction. Ordinary integer and memory instructions in branch,
call and return delay slots are compiled in the same block. Slot guards publish
the pending slot only when falling back, preserving the branch target, PR, PPC
and exact consumed cycles. PC-relative slot operands, DT lookahead and unsupported
instructions retain the interpreter path.

Conditional branches without delay slots can continue through their not-taken
path without returning to the dispatcher. Taken edges have separate register
snapshots and cycle costs. Closed loops, including eligible non-NOP delay slots,
check the cycle budget before every repetition. Entry checks reserve enough
cycles for every exit, including a taken branch at the end of a block. CPU clock
rates and emulated cycle costs follow the existing interpreter, including its
busy-loop shortcuts.

The CV1000 driver explicitly exposes one 64 KiB RAM window behind its speedhack
read handler. Ordinary reads may access this RAM directly; the watched longword
continues through the handler. Changing a read handler disables this mirror
until the driver registers it again.

The driver also registers its 16 MiB main-RAM address span with `Sh3SetDrcRam`.
Registration validates every read/write page, including the speedhack mirror,
against the supplied backing allocation. Older boards mirror 8 MiB in this span;
type-D boards such as `ddpsdoj` use 16 MiB. Code running in this RAM can use shorter
address checks and embedded host pointers instead of repeated page-table and
handler lookups. All external SH3 aliases, including the frequently used
`acxxxxxx` addresses, reach the same RAM. Internal addresses, device accesses,
unaligned operands and the speedhack's watched longword retain the interpreter
path. Constant PC-relative RAM operands use a direct pointer but still load the
actual memory value on every execution; literal data is not folded into code.

Remapping any read/write page in the registered span, replacing its read-mirror
handlers or changing the mirror contract revokes direct RAM access and flushes
the compiled blocks. Registration failure keeps the existing generic mapper.

## Code cache and state

The code arena is 8 MiB. A four-way lookup table holds 32,768 block records.
Blocks validate their source bytes and fetch mapping on entry. Native stores
overlapping the currently executing block's validated source bytes exit before
performing the store. The range includes DT lookahead and partial longword
overlaps; it also covers writes through RAM aliases. Ordinary data sharing the
same page can be written natively. Other modified blocks are detected
when next entered, including writes made by DMA or cheats. Compiled loops rely
on the same guard; device work remains outside those loops.

Reset and state loading clear translation metadata. No generated code, pointers
or lookup metadata are serialized. The existing CPU scan field order is retained.
Allocation failure falls back to the interpreter. Metadata is released at exit.

The Xbox code buffer uses aligned static storage, following the executable-cache
approach already used by other Salvia PPC cores. Updated code receives `dcbst`,
`sync`, `icbi`, `sync`, `isync` with 128-byte Xenon cache-line alignment. The
[PowerPC instruction-cache documentation](https://www.ibm.com/docs/en/aix/7.2.0?topic=set-icbi-instruction-cache-block-invalidate-instruction)
describes the required visibility and instruction-refetch sequence.

## Verification

The ROM-free differential test in `tests/sh3_ppc` executes the generated PPC
machine code under QEMU and compares it with the original interpreter. The
current implementation passes 660,696 instruction/block/edge cases. These include
compiled delay slots, branch/PR/T dependencies, slot guards, adjacent conditional
and memory exits, short cycle budgets, exact source-range boundaries and aliases,
plus verified RAM registration, mirrored backing, mutable literals and revocation
of compiled pointers after memory-map or handler changes.

A separate local `ddpsdoj` regression covers 3,600 frames from cold start through
first-stage combat, comparing per-frame RGB565 video and stereo audio, plus the
complete final save state. Runtime engine switching is checked over 600 frames
from a common gameplay state. A further 600-frame comparison saves/restores state
with two render cores active; audio, video and the final state match. The normal
Linux interpreter also passes a 600-frame regression. ROMs, save states and private test fixtures are
kept outside the repository.

These checks establish equivalence to the existing interpreter. The user tested
the initial Xbox backend at approximately 30 FPS during bullet-heavy gameplay,
and reported 30–40 FPS for the branch-path update. The direct-RAM update still
needs console performance testing. QEMU timings must not be reported as Xbox
FPS. This backend covers SH3/CV1000, not other emulated CPUs within FBNeo.

## Branch-path measurements

The same 600-frame gameplay segment, inputs and two render cores were used for
both versions. These are execution counts, not frame-rate improvements:

| Counter | Initial backend | Branch-path update |
| --- | ---: | ---: |
| Instructions executed by interpreter | 22,587,153 | 1,828,608 |
| Of those, delay-slot instructions | 20,729,032 | 604,244 |
| Calls into native blocks | 33,868,714 | 28,793,731 |
| Compiled blocks, including recompilation | 121,284 | 150,688 |

The larger compiled paths increase code-cache pressure; the arena remains 8 MiB.
Host QEMU wall time did not improve in these runs, which include both SH3-to-PPC
compilation and QEMU translating the emitted PPC again. Hardware measurement is
needed to determine the net performance change on Xenon.

## Direct-RAM measurements

The same 600-frame gameplay segment and inputs give these execution counts:

| Counter | Branch-path update | Direct-RAM update |
| --- | ---: | ---: |
| Compiled blocks, including recompilation | 150,688 | 93,659 |
| Full code-arena recycling events | 16 | 7 |
| Instructions executed by interpreter | 1,828,608 | 1,828,608 |
| Calls into native blocks | 28,793,731 | 28,793,731 |

The shorter memory paths reduce generated code size and cache pressure without
changing the number of instructions falling back to the interpreter in this
segment. They do not change guest CPU speed, frame skipping or blitter timing.
These counts cannot be converted into an Xbox FPS prediction.

## Related implementations and GPU scope

Source review references (the implementation above retains FBNeo's own timing,
register state, memory guards and interpreter fallback):

- [nullDC-360 block compiler](https://github.com/gligli/nulldc-360/blob/7b0bd50aa4a69e4f19306c9225146106f4387750/nullDC/dc/sh4/rec_v1/basicblock.cpp):
  PowerPC block linking and preservation of branch conditions across delay slots.
- [PicoDrive SH2 compiler](https://github.com/libretro/picodrive/blob/1890c2932234c9d30f4cd3851d02228baae8f09e/cpu/sh2/compiler.c):
  delay-slot dependencies, register caching, constant propagation and block links.
- [CV1000 hardware research](https://github.com/buffis/cv1k_research): blitter
  behavior and timing references for any future renderer implementation.

Salvia already uses the Xbox GPU for final presentation, scaling and shaders.
CV1000's EPIC12 still composes sprites in the CPU worker. Its drawing operations
read and write shared VRAM, including destination-dependent integer blending;
moving them to the GPU requires a renderer with correct ordering, overlap,
readback and save-state handling. nullDC-360's
[Xenos renderer](https://github.com/gligli/nulldc-360/blob/7b0bd50aa4a69e4f19306c9225146106f4387750/plugins/drkPvr/xenosRend.cpp)
targets Dreamcast PVR using libxenon, so it cannot directly replace this XDK-backed
CV1000 path. This update changes CPU translation; it does not add a GPU blitter.
