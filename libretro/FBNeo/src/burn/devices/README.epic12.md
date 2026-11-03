# CV1000 effect rendering on Xbox 360

## Scope

`epic12_fast_blit.h` accelerates source-mode 0 / destination-mode 0 effects in
Xbox builds. It is also enabled by `FBNEO_RENDER_THREADS_TEST` and
`EPIC12_BLIT_TEST` for the regression harnesses. The CPU fallback retains the original functions for other blend modes.
`epic12_gpu.h` and `epic12_gpu_xbox.h` add an Xbox-only Xenos compositor,
submitted by the existing ordered blitter worker. Other hardware drivers do
not use this compositor.

A 600-frame `ddpsdoj` gameplay sample contains about 304 million clipped candidate
pixels. Fixed-alpha blends account for about 172 million, or 56% of that work,
including tinted additive effects. Transparent candidates still incur the source
read and transparency test even when they do not write a pixel.

## Integer path

A 64 KiB table composes the existing tint and source-alpha operations. Both
rounding/clamping stages are preserved: clamp(floor(colour*tint/31)), then
floor(result*alpha/31). Neutral tint 32 is identical to the untinted result for
all 32 input levels. The table is derived from the existing colour tables at
reset; it is not included in save states.

When destination alpha is 31, the destination colour needs no multiply lookup.
The scaled source and destination are added as packed RGB words. Spare bits
between the 5-bit channels detect overflow, allowing exact saturation with word
arithmetic instead of three additional dependent table reads. Other destination
alphas retain their exact original multiplication lookup.

Clipping, source-wrap rejection, source transparency flags, draw order and
emulated blitter delay are unchanged. Pixels are still processed in their
original order, including source/destination overlap. No native CPU frequency,
frame skipping, game clock or emulated blitter timing is changed.

## Verification and measurements

`tests/epic12_blit` checks 65,536 component-table cases, 1,000,000 packed sums and
5,000 rendered-image cases against the original implementation. A separate
600-frame game run produces identical per-frame RGB565 video and stereo audio,
and an identical complete final save state including VRAM.

In the local PowerPC/QEMU profile, the blitter median decreased from 4.78 ms to
3.59 ms per frame. CPU emulation and drawing run concurrently; the SH3 side still
dominates that host profile. This measurement does not predict Xbox FPS and does
not establish the console's current bottleneck. The Xbox build needs a matching
hardware gameplay comparison, especially during halos and explosions.

## Xenos compositor

`GPU Blitter (Xbox 360)` is a CV1000 DIP option, default **On** (Dip D bit 0x20
clear). Off uses the CPU renderer. Changes are sampled at the next command list;
no game restart is required. The source shaders are embedded in the core, so no
new shader asset files are needed. Scaling/CRT filters still receive a completed
RGB565 image through the normal frontend path.

The compositor queues ordered, clipped draws into a region at most 512x512.
An 8 MiB atlas caches 128 source pages of 128x128 pixels. A batch is flushed
before a later source reads pending results, before CPU uploads/draws, at the
atlas/command/region capacity limit, and at the end of every command list.
In-place overlap and source wrap requiring the software pixel order fall back
to the CPU. GPU resources and cache tags are not serialized; CPU VRAM is complete
when the existing worker wait returns. Loading a state or resetting invalidates
cached pages. Small batches (fewer than 8192 candidate pixels) stay on the CPU.

Supported operations are plain/tinted draws and source mode 0 with destination
modes 0, 1 and 4. The pixel shader preserves the two tint/alpha truncation stages.
Destination mode 4 is normalized to mode 0 with its complementary alpha.
Full-destination additive halos use hardware ONE+ONE blending; other supported
mixes sample a GPU-only snapshot of the affected destination rectangle. Resolve
rectangles are aligned to 8 pixels. These feedback operations do not wait for a
CPU readback. Consecutive draws with the same hardware blend and feedback class
share one RECTLIST submission (up to 256 rectangles), even when their tint,
source/destination alpha or transparency flags differ. Each atlas piece uses
three 16-byte vertices. Position/atlas coordinates are native unsigned 16-bit
pairs; the GPU converts and normalizes them, avoiding CPU float conversions.
Two packed UBYTE4 words carry tint/alpha and mode per rectangle. All corners
carry identical values, rounded back to integers in the pixel shader before
the original two-stage colour arithmetic. UBYTE4 uses the documented DWORD
packing (x in the low byte); USHORT2 uses native 16-bit elements. Their Xenos
endian conversions differ. No per-group pixel constant uploads are needed on
this path. The previous uniform-constant GPU path remains a compatibility
fallback, using float vertices and the original grouping rules.
Feedback draws share a snapshot only when their exact destination rectangles do
not overlap. Lookahead is capped at 64 draws, and the union resolve area cannot
exceed twice the sum of the separate areas. These limits bound CPU planning work
and prevent sparse sprites from causing large transfers. The shared planner and
atlas geometry live in `epic12_gpu_batch.h`.

Each batch ends in a tiled Resolve and one synchronized, readonly
LockRect. The fixed 512x512/32bpp transfer in `epic12_gpu_untile.h` traverses
32x32 source blocks, loads four adjacent pixels with VMX, applies the colour
mask, and stores directly into CPU VRAM. This fuses untile and masking into
one pass. SDK-generated block/local address tables occupy 1.5 KiB. Destination
alignment and partial vectors are handled explicitly; no pixels outside the
requested rectangle are written. Prefetch touches only cache lines containing
pixels in the locked rectangle. The readonly lock/unlock still provide GPU
synchronization and CPU cache coherency. The original XGUntileTextureLevel plus
scalar mask path remains available if validation fails.
Masking channels to multiples of 8 recovers EPIC12's exact 5-bit saturation,
including repeatedly saturated additive results. The transparency marker is
copied separately from the source, and transparent fragments are discarded.

The attribute renderer selects a short pixel shader for plain/replacement and
additive groups, and a separate destination-reading shader for feedback groups.
Neither path changes the planner, resolves, draw order, or either colour
truncation stage. The unified attribute shader and uniform-constant shader remain
fallbacks. In the 600-frame gameplay trace, 91.3% of candidate pixels use the
short shader. XDK compilation reports 18 ALU instructions for it and 27 for the
feedback shader, compared with 39 for the unified shader. These are static
compiler results, not measured GPU execution time or Xbox FPS.

The SDL bridge holds the same critical section as Present, the loading watchdog
and device reset. Offscreen rendering saves/restores device state, target and
depth surface. It borrows EDRAM base 0 between frontend presentations; the SDL
frontend clears and redraws its backbuffer before every Present. Device reset
increments a generation number; the core then recreates resources.

Resource creation includes a one-time GPU pixel self-test against the CPU
renderer (192 draws covering plain, tint, additive, feedback, alpha marker, flips,
page boundaries and merged groups with different per-sprite styles). It runs once
per game/device generation, with no per-frame comparison. If the new attribute
path fails, the unified shader, SDK transfer, and compatibility GPU path are
tested in turn; only failure of those paths
disables GPU rendering for the generation. Details are appended
to `game:\cv1000-gpu.log` (beside the running `fbneo.xex`); the first open
explicitly creates the file because libretro's append mode requires an existing
file. Session command/draw/resolve counters are written when gameplay pauses
(menu or overlay), as well as on game exit. The SDL callback is registered and
unregistered on the emulation thread; it joins the blitter worker before reading
counters and does not hold the GPU lock while waiting. Other cores leave the
callback unset.

One in 64 GPU batches samples CPU wall time for lock acquisition, source/target
upload, submission, readback wait, and untile/masking. Averages and the peak
sampled total are written with the counters at pause/exit. No GPU timestamp
queries or extra fences are inserted. These times include CPU/GPU waits and do
not separately measure GPU execution, SH3 emulation, or total frame time.
Libretro
also displays an active/off/CPU-fallback notice when the status changes. The
worker publishes an atomic status code; the emulation thread delivers the UI
message after its frame callback. There is
no periodic disk logging during play. The backend uses about 18 MiB of texture
memory with the 256-page source cache, or 10 MiB with its 128-page fallback,
plus a temporary 4 MiB buffer during self-test.
The fused transfer also runs an independent native VMX self-test using about
2 MiB of temporary memory before the GPU test. It covers all 512x512 source
addresses, odd destination pitches, unaligned stores and rectangle guard pixels.
Its buffers are freed before the GPU test, and it never touches game VRAM.

## GPU verification

`tests/epic12_gpu` checks 6,291,456 shader component combinations against integer
reference formulas, and compares 14,400 ordered draw commands against the existing
software renderer, including full VRAM and emulated delay. It covers clipping,
flips, transparency, source wrap, overlap, framebuffer reads, CPU upload barriers,
live disabling, injected GPU failure, and a 260-page atlas capacity sequence.
A further 640 commands check uniform grouping, shared-snapshot overlap/area
barriers, complementary-alpha normalization and submission capacity. Another
640 commands change each sprite's tint, alpha and transparency, and compare both
attribute and compatibility paths with the CPU result. The mock
uses the same atlas vertices as the native backend, samples the generated
RECTLIST coordinates, decodes packed per-vertex parameters, and reads an actual
per-group snapshot. It models UNORM
additive saturation and feedback. It is not a Xenos driver
emulator and cannot establish console performance.

The private ROM harness also runs `ddpsdoj` from a common gameplay checkpoint,
including an intermediate save/load, and compares every video/audio hash and
complete final state with the CPU version. XDK compilation checks the native
backend and all seven shader entry points. Actual driver behavior is checked by
the on-console self-test; gameplay visuals and frame rate still require Xbox
hardware testing. No console FPS gain is claimed from host timings.

For the same private 600-frame battle sample, batching reduces sprite draw calls
from **397,829 to 141,956** (64.3%) and feedback resolves from **40,759 to 27,427**
(32.7%). Resolved feedback area is 43,027,072 pixels vs. 43,091,392 previously;
CPU readbacks remain 861 batches. These are command-trace counts, not measured
Xbox frame-rate gains. Every video/audio hash and the entire final state match.

The per-sprite attribute revision reduces those **141,956 submissions to 81,465**
(a further **42.6%**) in the same 600-frame sample. Feedback resolves fall from
27,427 to 27,016; their copied area rises from 43,027,072 to 44,119,936 pixels
(2.5%) as more neighbouring sprites share snapshots. The number of source
rectangles, candidate pixels and CPU readbacks is unchanged. All 600 per-frame
video/audio hashes and the final state/VRAM remain identical to the CPU reference.

## Transfer and shader revision

The supplied console log confirms the attribute renderer passed its GPU test.
Its 218 sampled batches average 0.001 ms acquiring the lock, 2.442 ms uploading,
0.530 ms submitting, 4.738 ms waiting for readback, and 3.417 ms untiling/masking.
Those are CPU wall times per GPU batch, not per complete game frame. This
measurement motivates the fused VMX transfer and shorter pixel shaders above.
The new log's `pipeline shaders=... transfer=...` line identifies which validated
paths the console is actually using. No new frame-rate claim follows from the
old log or from the compiler's shader estimates.

`tests/epic12_gpu_untile` checks 262,144 addresses and 6,820 rectangles against an
independent addressing reference, with output guard regions and little-endian
sanitizer / big-endian PPC vector-model runs. `tests/epic12_gpu` checks both
specialized shader formulas against the integer reference, including the unified
fallback and plain draws. The 600-frame gameplay regression still matches every
core video/audio hash and the complete final state. Frontend pitch-preserving
audio time stretching has separate tests under `tests/audio_tempo`.

## Source atlas residency (Xbox 360)

The source atlas retains 128x128 VRAM pages between batches. The current
implementation first pins every resident page required by the entire next batch,
then uploads misses into empty slots or evicts the least recently used unpinned
page. This replaces the old first-unpinned policy, which repeatedly discarded
useful sprite data even when many other entries had not been used recently.
The shared `epic12_gpu_cache.h` implementation is also used by the test model.

The backend requests a 2048x2048 linear atlas (256 pages, 16 MiB), after mandatory
GPU resources are allocated. Allocation failure uses 2048x1024 (128 pages,
8 MiB) with the same improved eviction policy. A failed startup pixel test with
the larger atlas also retries 128 pages before the existing shader/transfer
fallbacks. The maximum distinct source pages in one batch stays at 128.
Vertex shader atlas scaling and the uniform renderer use the actual texture
height; the startup pixel test places its source pages in the last atlas row
to exercise the larger coordinates on the real GPU.

CPU uploads, software draws and GPU writeback invalidate overlapping cached
source pages. Wrapped writes, reset, device recreation and loading a state clear
all tags. No command ordering or emulated blitter delay changes are needed.
Unsigned LRU timestamp overflow preserves age order by compacting ranks.

The pause/exit GPU log adds `atlas slots=... hits=... upload_pages=...
upload_bytes=... input_pixels=...`. Each uploaded page is 64 KiB. Startup
self-tests are excluded from these counters, as from the existing draw and
phase timing statistics. These counts distinguish source cache misses from
the destination image upload, which remains necessary for each batch.

In the common 600-frame battle regression (including midgame save/load), the
persistent pixel-copy model reports **14,603 uploaded pages with the previous
128-slot policy versus 907 with 256-slot LRU**, a 93.79% reduction (912.69 MiB
versus 56.69 MiB). Both policies observe the same actual CPU/GPU invalidations
and resets. Every core video/audio hash and the complete final state match
the CPU reference. Batch, draw, feedback resolve and readback counts remain
unchanged. This measures avoided source upload work, not console FPS or GPU time.

## Transparent borders, feedback reuse and tiled source atlas

The September 27 follow-up hardware log confirms the 256-page cache is active:
source/target upload averages 0.716 ms per sampled batch, compared with 2.468 ms
in the previous session. Readback wait averages 5.323 ms and now dominates.
Sessions contain different gameplay, so these numbers locate work rather than
establish a frame-rate comparison. Readback wait may also include earlier GPU
commands, including presentation synchronization.

Each uploaded atlas page now stores 128-bit alpha masks per source row and OR
summaries for each eight rows (`epic12_gpu_alpha.h`, 2304 bytes/page; 576 KiB for
256 slots). Queries use this metadata, not repeated VRAM pixel scans. Only
transparent draws are cropped to the exact opaque-source bounding box; source
and destination are adjusted together for both flips. Nontransparent draws
retain zero-alpha pixels, which still overwrite destination data. Empty draws
are omitted. Draw order and emulated blitter delay remain unchanged, and the
input/readback rectangle is reduced to the surviving destination union.

Feedback snapshots can also span later draw groups. A maximum 32-command
lookahead accepts only complete feedback groups whose destinations do not
intersect ANY earlier write in the window, including intervening plain and
additive draws. The expanded snapshot must stay within 1.25 times the sum of
its component resolve areas. Each original draw group still keeps its own
shader/blend state and submission order.

The source atlas uses native tiled A8R8G8B8 storage for 2D texture sampling. A
VMX uploader visits 32x32 blocks and issues sequential aligned stores into
write-combined memory, without reading that memory, using unaligned destination
stores or zeroing destination cache lines. Layout tables come from SDK address
helpers. Source alpha metadata is still built only on cache misses. Driver
LockRect/UnlockRect synchronization and cache handling remain in place.
A failed layout/pixel self-test falls back to linear texture storage. Further
pixel-test retries can disable snapshot lookahead, alpha cropping, the large
cache, split shaders and the fused readback, retaining the previous renderer.

The common 600-frame battle regression (with midgame save/load) keeps every
core video/audio hash and the complete final state identical. Compared with
the previous cached renderer:

| Work | Previous | This revision |
| --- | ---: | ---: |
| Candidate sprite pixels | 303,138,483 | 255,949,287 (-15.57%) |
| Feedback resolves | 27,016 | 21,893 (-18.96%) |
| Feedback resolve pixels | 44,119,936 | 34,707,904 (-21.33%) |
| Draw submissions | 81,465 | 78,650 (-3.46%) |
| Readback pixels | 92,321,705 | 91,932,895 |

These are workload counts from the software GPU model, not console FPS. Tiled
sampling performance is not predicted by that model and needs hardware testing.

`work alpha=cropped snapshots=lookahead atlas_layout=tiled` identifies the new
paths in the console log. `raster_commands` and `raster_pixels` count the work
after cropping; `commands` and `pixels` retain the original queued workload.
`readback_pixels` now counts the actual reduced rectangles. Phase timing adds
`alpha_plan` for per-batch cropping; `upload` remains source plus input upload
and includes generating alpha metadata on source-cache misses. Startup tests
are excluded. No extra per-frame GPU synchronization or disk writes are added.

Independent suites in `tests/epic12_gpu_alpha` and `tests/epic12_gpu_tile` check
metadata queries and layout conversion on sanitized hosts and big-endian PPC.
Native XDK probes confirm the vector stores and count-leading-zero intrinsic.
The on-console pixel test adds sparse/empty sprites, opaque zero-alpha draws,
flips, cross-group feedback reuse, cache replacement and the final atlas row.

## 2026-09-27: Disjoint draw grouping, cached alpha bounds, and async presentation

The next console log confirms all GPU6 paths were enabled. Its last sample was
source+input upload 0.999 ms, alpha planning 0.350 ms, submission 0.509 ms,
readback wait 4.982 ms, and untile 0.529 ms. These were CPU elapsed times from
every 64th batch; fixed-period sampling could select the same position within a
frame repeatedly. The wait also includes preceding GPU commands, including the
frontend Present's presentation-interval barrier. It is not solely shader time.

GPU7 adds three separate changes:

- Attribute commands may move forward by at most seven positions per selection
  to join the preceding draw type. A candidate must be disjoint from EVERY
  crossed destination rectangle. Thus every overlapping pair retains original
  order. Source atlas data is fixed for the batch; all command fields and
  emulated delay are unchanged. Uniform fallback retains original ordering.
  The startup pixel test can disable reordering independently.
- Alpha source-rectangle bounds are cached with each page's metadata; rebuilding
  a row invalidates cached results. This avoids rescanning an unchanged sprite's
  mask whenever another instance is drawn. Empty, flipped, and cross-page
  results retain the original semantics.
- Only the active CV1000 GPU compositor requests SDL async presentation. The SDL
  adapter preserves VSync and the existing frame limiter, and owns an extra
  front-buffer texture instead of blocking subsequent GPU commands behind the
  synchronous Present barrier. Resource and queue failure retain the synchronous
  path. Detailed lifecycle and tests are documented with the SDL adapter.

Sampling now uses xorshift at approximately 1/64 frequency and separates the
first batch after Present from later batches, and normal from async presentation.
Source upload, alpha/reorder planning and destination input upload are distinct
in the new `batch_timing` records. Counters exclude startup self-tests. Timing
records still measure CPU elapsed time, not hardware GPU timestamps or whole-frame
FPS. Real-console validation remains necessary.

Final 600-frame gameplay regression (with mid-run save/load) matches the
original per-frame core audio/video hashes and full final state. Relative to
GPU6: draw submissions 78,650 -> 56,200 (-28.54%); feedback resolves
21,893 -> 21,489 (-1.85%); feedback pixels 34,707,904 -> 35,306,752 (+1.73%).
The small copied-area increase trades for fewer draw state transitions.
Alpha metadata reads 4,424,182 -> 501,856 (-88.66%), with 357,003 hits and
40,826 misses (89.74%) including page invalidation at save/load. Source uploads,
original commands, cropped raster pixels and output readback pixels are unchanged.

Alpha standalone tests cover host ASAN/UBSan, big-endian PowerPC, hash collisions,
empty results and invalidation. The GPU combination gate covers both atlas sizes,
all shader fallbacks, source writes and alpha/snapshot/reorder combinations.
SDL's pure C async policy has host sanitizer and PowerPC lifecycle tests; actual
Xenos/Guide/display behaviour remains a real-console validation item. The
seven shader sources remain byte-identical to GPU6 and reuse that FXC validation.

Guide/timeout fallback is sticky for that request, preventing repeated mode
switches. A new game or GPU Blitter Off/On request may re-enable async after the
system condition clears. Presentation state is now folded into the pause-time
`game:\cv1000-gpu.log` report rather than written from the live Present path.

## 2026-09-29: pause-only bottleneck diagnostics

The diagnostic revision does not change draw order, shaders, emulated timing or
the GPU batching policy. Runtime instrumentation is limited to integer counters
plus xorshift-selected timing samples at approximately 1/64 frequency. No new
file is written while gameplay is running.

The existing pause callback joins the CV1000 worker and then appends the summary
to `game:\cv1000-gpu.log`. New records include:

- `flushes`: batch termination counts for end-of-list, source-page capacity,
  512x512 region growth, source/destination dependency, command capacity,
  CPU uploads and CPU-rendered fallback draws; it also reports small CPU batches
  and GPU backend fallbacks.
- `worker_wait`: sampled exposed wait time when the emulation thread joins the
  persistent blitter worker. The worker remains pinned to Xbox hardware thread 2
  by `BurnRenderWorker(..., 1)`.
- `present_state`: the async-present request/active/failure/resource state and
  cumulative activation/fallback/timeout counters.

Presentation transitions no longer create `cv1000-present.log`; this keeps the
existing three-file diagnostic workflow and removes live presentation file I/O.

## 2026-09-29: precise source dependency barrier

The first pause-only Xbox trace showed that presentation and worker overlap were
already healthy: asynchronous presentation stayed active with no fallback or timeout,
sampled readback wait averaged about 0.49 ms, and the exposed worker join averaged
about 0.01 ms. The dominant batching barrier was instead the conservative
`source_dependency` check: 36,419 dependency flushes and 40,423 small CPU batches
versus 5,552 region, 1,167 source-page and 164 command-capacity flushes.

Previously a source rectangle intersecting the union destination bounding box forced
a flush, even when it only touched empty space between queued destination rectangles.
The new path keeps a 2,048-entry 128x128 destination-page bitmap for the current
batch. If the source touches no written destination page the dependency is rejected
immediately. A page hit is only a coarse candidate; the code then checks the source
against every queued destination rectangle and flushes only on a true rectangle
intersection. Full uncropped destination rectangles are used deliberately, so
transparent pixels can only make the test conservative.

The page filter does not replace the exact dependency rule and therefore cannot
permit a true read-after-write hazard. A 200,000-case randomized rectangle test over
the complete 8192x4096 VRAM found zero cases where exact overlap lacked a page hit.
The pause report adds `dependency bbox_checks/page_hits/exact_hits/avoided_false_flushes`
to quantify the saved barriers on hardware.

The Xbox follow-up reported `bbox_checks=22835`, `page_hits=22835`,
`exact_hits=22835` and `avoided_false_flushes=0`. The conservative bounding-box
barrier was therefore already exact for that workload. The destination-page bitmap
and exact-rectangle scan were removed from the final optimized build so they do not
add CPU overhead without reducing a single flush.

## 2026-09-29: dynamic 128/256-page batch capacity

The same hardware trace reported 2,746 `source_pages` flushes while the active
source atlas had 256 slots. The outer batching rule and native backend still capped
each batch at 128 distinct source pages, leaving half of an already allocated
16 MiB atlas unavailable to one batch.

The batch source-page limit now follows the cache capacity that actually survived
resource creation and pixel validation. A 256-slot atlas permits 256 distinct source
pages in one batch; the existing low-memory 128-slot fallback retains the old
128-page limit. No additional atlas allocation is introduced.

The shared CPU model uses the same dynamic limit. Its 260-page stress sequence is
specified to split into three batches with 128 slots and two batches with 256 slots.
The Xbox log now prints `batch_page_limit` beside `atlas slots` so hardware can
confirm which policy is active.
