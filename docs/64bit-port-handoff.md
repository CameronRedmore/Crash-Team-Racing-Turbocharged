# 64-bit / ARM64 port: handoff

> **2026-10-06: this branch (`64bit-2026.10`, worktree `src/worktrees/64bit-2026.10`) supersedes `64bit`.**
> It is `turbocharged` @ e15b395bd (44 commits past the old base) with `64bit` merged in. Conflicts were resolved
> to the turbocharged side, then `tools/ctr64/rewrite2.py --apply` (paths pointed at this worktree and `/tmp/b64n`)
> re-applied P32 accesses to the new code, and the rest was fixed by hand. State: 64-bit Debug (clang) builds, `ctest`
> 13/13; 32-bit gcc builds with no warnings, `ctest` 13/13; the 64-bit game boots headless to the title, main menu,
> adventure hub (modern minimap, HUD icons, title logo all draw) and the warp-pad race menu. **A/B against 32-bit SSE has not been re-run on this branch.**
> Bugs the compiler could not catch, found by booting: `P32_GET(T **, slotArray)[i]` (wrong stride, 4 sites: `lngStrings` x3, `ptrTexLayout`),
> `visInstSrc` walks in `LevInstDef.c`, `(uintptr_t)` on an int that holds a handle (`ptrPushBufferUI`). Grep for `P32_GET\([^,]*\*\*` after every merge.
> Tests that store addresses in game fields need `static` objects (stack is outside +-2 GiB). The old `64bit` branch has uncommitted unrelated edits; leave it alone.
> Merge recipe for future turbocharged updates: merge, resolve to turbocharged, build 64-bit, run rewrite2, fix the leftovers, boot-test.

Branch `64bit`, worktree `src/worktrees/64bit`, based on `turbocharged` @ 8cbfb7797.
First written 2026-10-03, updated twice later the same day. Nothing is pushed.

## Status

- The 64-bit x86-64 Linux build (clang, Debug and Release) compiles with no errors and links. `ctest` passes 5/5.
- The 32-bit build (gcc `-m32`) compiles with no warnings and `ctest` passes 5/5 after every commit.
- The 64-bit game (Debug and Release) boots headless through the intro and main menu, loads the adventure hub, drives into a warp pad and plays a Relic Race on Crash Cove with correct rendering and HUD. No `CtrPtr32_RangeError` aborts on that path.
- **A/B against 32-bit has started and is clean so far** (commit `c7940a4fa`, see "A/B testing" below). On a 12k-frame replay (boot, menus, adventure hub, warp pad, Crash Cove relic race) the 64-bit build matches a 32-bit SSE build on every frame: timers, all RNGs, driver position/rotation/speed/items, and every thread bucket's thread flags, instance flags and instance matrices. The first 11k frames of a 28k-frame idle attract-mode replay (AI hub demos with different characters) also match. No port bugs found yet.
- Not yet done: wider A/B coverage, custom racers, other platforms. See "Next steps".

## Goal

Build and run the game on 64-bit targets (x86-64 Linux first, then ARM64, macOS, Windows x64) using the most portable method available. That rules out low-memory tricks (`MAP_32BIT`, `-no-pie`, `__ptr32`) and anything tied to clang or MSVC. The 32-bit build must keep working unchanged.

## Design

Retail-shaped structs keep their 4-byte pointer fields. On 64-bit builds each such field is a `CtrPtr32` handle holding a signed 32-bit offset from an origin inside `gCtrPtr32Anchor`. Because layouts are identical, hex offsets, JitPool item sizes, the `CTR_STATIC_ASSERT`s and in-place file patching keep working.

Handles only work if every pointee is within +-2 GiB of the anchor. That holds for statics, string literals, functions and the mempack arena (a static array). It does NOT hold for `malloc`ed buffers, stack addresses or SDL memory, so those cannot be stored in game structs (see "Open problems").

API, all in `include/ctr_ptr32.h` (identity on 32-bit):

| Macro | Meaning |
|---|---|
| `P32(T)` | declare a pointer field of type `T` (`T` is the full pointer type, e.g. `struct Model *`) |
| `P32_FNPTR(ret, name, (args))` | declare a raw function-pointer field |
| `P32_GET(T, lv)` | read `lv` as a pointer of type `T` (casts on both builds) |
| `P32_SET(lv, v)` | store pointer `v` into `lv` (statement use only; its value is not a pointer) |
| `P32_ENC(v)` / `P32_DEC(T, h)` | convert a raw pointer to/from a handle kept in an int |
| `P32_TRY_ENC(v, &out)` | like `P32_ENC`, but returns 0 instead of aborting |
| `P32_DEFER(e)` | static-initializer value: `(e)` on 32-bit, `0` on 64-bit |
| `CTR_P32_STATIC_FIXUP(name)` | constructor that stores deferred static pointers at startup |
| `CTR_P32_MUTABLE` | `const` on 32-bit, empty on 64-bit (for data patched at startup) |

Properties the code relies on:

- **Sentinels.** Values in [-16, 16] are stored as themselves, so `(fnptr)-2` (`LOAD_QUEUE_CALLBACK_SET_POINTER`), `(char *)-1` and small integers round-trip. The origin is `&gCtrPtr32Anchor.bytes[32]`, so no real object is that close to it.
- **Linearity.** `enc(p + k) == enc(p) + k`, so int arithmetic and comparisons on handles behave like they did on addresses.
- **Low bits.** The anchor is 8-byte aligned, so `h & 3` equals `p & 3` (alignment tests and low-bit tags work on handles).
- **Stability.** Handles to objects in the image do not change between runs (PIE moves the whole image), which the checkpoint code uses.

`gCtrPtr32Anchor` and `CtrPtr32_RangeError` live in `platform/native_ptr32.c` (included by `native_memory.c` and by standalone tests). The range check aborts with the offending pointer; it is the main tool for finding anything missed.

### Rules used throughout

1. **Pointer fields in retail-shaped structs** (`include/`): `P32(T)`.
2. **Pointers to arrays of retail pointer slots** (in-file tables, `ICONGROUP_GETICONS`, `ST1_GETPOINTERS`, `ANIMTEX_GETARRAY`, `lngStrings`, `visInstSrc`, `ptrModelsPtrArray`, ...): walk them as `P32(T) *` and read elements with `P32_GET`. A `T **` view would use an 8-byte stride.
3. **Ints that carry pointers in retail-shaped storage** (`int`/`u32` fields, scratchpad `*Ptr32` words, `sdata->ptrMPK`, `idpp->otRangeNormal`, ...): hold a handle. Convert with `P32_ENC`/`P32_DEC` at the boundary.
4. **Purely local address arithmetic**: `char *` or `uintptr_t`.
5. **Fields that hold retail codes, not host pointers**: plain `u32`. Found so far: `Instance.funcPtr[]` (retail draw-function addresses used as dispatch codes), `ChannelAttr.spuStartAddr` (SPU RAM address), cutscene `CsOpcodeArg` branch targets (retail overlay addresses, translated by `CS_ScriptCmd_OpcodeAt`).
6. **Native-only structs** that never alias retail memory keep real pointers (`RenderBucketEntry`, RenderBucket contexts, `NativeDrawLevel`, audio, CD). Native structs that overlay retail memory (scratchpad, `gGT->DecalMP`) use `P32`: `ParticleRenderListScratch`, `VehGroundShadowEntry/Scratch`, `DecalMPEntry`.

`LOAD_RunPtrMap` stores `P32_ENC(origin + offset)` into each patched slot. Level words that the renderer interprets as pointers (texture/mosaic words, quadblock texture slots) are decoded with `P32_DEC`.

Checkpoints (`platform/native_checkpoint.c`) treat "addresses" as whatever pointer slots hold: raw addresses on 32-bit, handles on 64-bit (`NativeCheckpoint_PtrToU32` uses `P32_TRY_ENC`). Region starts and the code anchor are recorded the same way, so the relocation logic is unchanged.

## Commits on this branch

1. `8e110e4b7` adds the `CTR_NATIVE_64BIT` CMake option, relaxes the `sizeof(void *) == 4` guards, and adds the `linux-x64-debug` preset.
2. `380bbb142` converts about 440 pointer fields in `include/` to `P32` and rewrites about 13k access sites.
3. `687779622` moves 775 static pointer initializers into startup fixups.
4. `bf2c94784` pass 2: macro-argument sites (426 errors to 131).
5. `4322dc925` hand-converted sites: `T **` aliases, scratch overlays, rendered-quadblock lists, load-queue set-pointer targets, sentinels, deferred fixups.
6. `8aae6ca94` checkpoint handles; the build links and tests pass.
7. `381a988dd` in-file pointer arrays and `LOAD_RunPtrMap`.
8. `11659fb74` pointer/int truncation (all `-Wpointer-to-int-cast` / `-Wint-to-pointer-cast` sites).
9. `edcbeb1fd` and its predecessor: bugs found by booting (render-bucket terminator, cutscene branch targets, particle icon reads).
10. `c7940a4fa` cross-build replay A/B tooling (skip-bootstrap playback, deterministic boot and CD, `--ab-trace`).

## A/B testing

Added in `c7940a4fa` (internal builds only, documented in `docs/REPLAYS.md` under "Cross-Build A/B"):

- `--replay-skip-bootstrap`: plays a replay without restoring its bootstrap checkpoint. Needed because checkpoints contain raw pointers (32-bit) or handles (64-bit) and do not restore across builds. Only accepted for replays recorded with `--record` and without `--toggle`; those are marked in header `reserved[0]` (`NATIVE_REPLAY_FROM_BOOT_MAGIC`). Use with `--replay-bypass-header`, since checkpoint sizes and build IDs differ.
- Deterministic boot: in from-boot record and skip-bootstrap playback, `VSync()`/`Platform_WaitUntilVBlank()` skip wall-clock catch-up before frame 0 (`NativeReplayScheduler_DeterministicBoot`). Without this the boot VBlank count, and with it `frameFinishedVRAM` and the 144 Hz `callbackFrame` phase, varied per run.
- Deterministic CD: while any replay records or plays, `NativeCD_PumpCallbacks` waits for the in-flight worker read, so loads finish on the same VBlank every run. Before this, two playbacks of one replay on the same binary diverged at the first load.
- `--ab-trace FILE`: one line per frame from `MainReplayTrace_Frame` (`game/MAIN/MainMain.c`). Bucket field format is `count:threadHash:instFlagsHash:matrixHash:modelIndexHash`. Driver and bucket data are skipped while `Loading.stage != -1`, because buckets are stale during loads; walking them crashed. Instances are only hashed if inside the instance JitPool, because the camera thread's `inst` points elsewhere.

Findings:

1. **x87 vs SSE.** The default 32-bit gcc build uses x87 floats. It drifts from SSE builds in native float code (seen as a 1-unit `posCurr.z` difference in the relic race, frame 10876, config has `smoothed_collisions=1`). The 32-bit `-msse2 -mfpmath=sse` build matches 64-bit exactly. **Always A/B 64-bit against `/tmp/b32sse`, not `/tmp/b32`.** This is a pre-existing cross-platform replay and ghost determinism issue, not a port bug. The user has not been asked yet whether shipping 32-bit x86 builds should switch to SSE2.
2. **Stale `modelIndex`.** `PROC_BirthWithObject` never sets `Thread.modelIndex`. Some threads (`saveobj` in STATIC, an unnamed OTHER thread with flags `0xc030d`) keep stale pool bytes there that look like the low half of a pointer. The value differs between any two builds, even 32-bit x87 vs SSE, so it is benign. That is why it has its own hash. The writer was not found.

Harness: `tools/ctr64/ab/` (`drive.sh`, `inner.sh`, `cmp.py`). The scratch dir is `/tmp/ctr64-ab` (override with `S=`). It needs `config.ini.orig`, `debug/`, `memcards/`, `mods/` and an `assets` symlink; copy them from the game dir as in the headless-testing notes. Do not call the env var `ARGS`, because `xvfb-run` uses that name. Use `GAMEARGS`. Example:

```
cd /tmp/ctr64-ab
# record (from boot, so no --toggle); kb1 presses F4 until the keyboard is player 1
BIN=/tmp/b32sse/ctr_native TAG=rec GAMEARGS="--record" ./drive.sh 12 kb1 sleep:12 Return sleep:15 Return \
  sleep:12 c sleep:5 c sleep:5 c sleep:30 hold:c:9 sleep:25 hold:c:20
RP=$(find debug/reports -name input.ctrreplay)
for v in b32sse b64; do BIN=/tmp/$v/ctr_native TAG=$v \
  GAMEARGS="--replay $RP --replay-skip-bootstrap --replay-bypass-header --ab-trace /tmp/ctr64-ab/trace_$v.txt" \
  ./drive.sh 5 wait; done
python3 cmp.py trace_b32sse.txt trace_b64.txt   # prints differing field counts and first differing frame per field
```

Playback runs at about real time under Xvfb (Debug 64-bit is slower), so a 28k-frame replay takes more than 10 minutes per build. Run long ones in the background. Replays in `/tmp/ctr64-ab/debug/reports/` are lost on reboot. `cmp.py` ignores the last line of each trace, which may be truncated if the game was killed.

Build `/tmp/b32sse` with:

```
cmake -S . -B /tmp/b32sse -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
  -DCMAKE_C_FLAGS="-m32 -msse2 -mfpmath=sse" -DCMAKE_EXE_LINKER_FLAGS=-m32
```

## Tools (`tools/ctr64/`)

They need libclang (`python3 -c "import clang.cindex"`) and a configured 64-bit build dir with `compile_commands.json`. `common.py` hard-codes `/tmp/b64` and the worktree path.

- `common.py` parses `main.c` (unity build).
- `rewrite2.py` pass 2 (applied). Dedupes sites a macro expands more than once.
- `rewrite_ckpt.py` pass 2 restricted to `native_checkpoint.c` (applied).
- `gen_fixups.py` static-initializer pass (applied, one-shot).
- `alias.py FILE A B VAR 'T *'`: rewrites `VAR[i]` reads/writes inside a line range.
- `retype_arrays.py`: retypes locals reported as `T ** = CtrPtr32 *` errors in `/tmp/b64.log`.
- `p32_arrays.py FIELD... -- FILE...`: fields that point at slot arrays.
- `uintptr_casts.py FILE...`: `(u32)(uintptr_t)p` to `P32_ENC`, `(T *)(uintptr_t)w` to `P32_DEC`.
- `b64.sh` builds `/tmp/b64` and summarises errors.

The tools are one-shot and not idempotent; do not re-run them on converted code.

## Next steps, in order

1. **Widen A/B coverage** with the tooling above. Done: boot, main menu, adventure hub, warp pad, Crash Cove relic race, and the first 11k frames of idle attract mode. Next: finish the idle attract replay (`/tmp/ctr64-ab/debug/reports/*/ctr-19*`, 27867 frames; the 32-bit SSE trace `idle_b32sse.txt` is already complete, so only the 64-bit playback needs rerunning). Then Arcade races with 7 AI on several tracks (AI exercises most gameplay code with just "hold accelerate"), battle, boss race, cutscenes (intro, podium, credits, garage), 2-4 player split screen, ghosts (Time Trial), memory card save/load, and checkpoints (F5/F8 within one build). Silent truncation and wrong-stride bugs show up as trace divergence, not crashes. If the trace misses something, add fields to `MainReplayTrace_Frame`. Pointer fields must stay out of hashes.
2. **Allocations that must be inside the image.** Custom racers `malloc` model/VRM buffers and store them in handles (`platform/native_custom_racer.c`: `NativeCustomRacer_LoadQueueSlot`, `NativeCustomRacer_LoadModelNow`, VRM buffers). On 64-bit these will hit `CtrPtr32_RangeError` as soon as a custom racer is used. Replace with a static pool (e.g. a bump/free-list allocator over a static array) for anything that ends up in a game struct. Same audit for `LOAD_Assets.c` native buffers.
3. **Stack addresses in game structs.** None hit so far. `savedStackPtr32` in the draw-level scratch stores a truncated stack address but is never read back. If one turns up, make the object static or run the game loop on a thread with a static stack.
4. **Code not compiled by the Linux 926 build.** Inactive `#if` regions were never parsed: `game/zRegionJapan/*` and other non-926 regions, Vita-only code (`native_adhoc.c` `__vita__` block), `BUILD == SepReview` paths. Vita and web stay 32-bit, so only regions that a 64-bit target compiles matter.
5. **Remaining warnings.** `platform/native_libgte.c` `NormalColorDpq`/`ColorDpq` pass an int where the GTE macro dereferences a pointer. They are unused and broken on every build.
6. **x87 decision.** Ask the user whether 32-bit x86 builds should use `-msse2 -mfpmath=sse` so replays and ghosts agree across platforms (see finding 1).
7. **Performance.** `P32_GET` adds a compare and an add per access. Not measured. Compare frame times of Release 32-bit vs 64-bit.
8. **Other targets.** ARM64 Linux, macOS arm64 (check the +-2 GiB assumption for the Mach-O image and `__DATA` placement), Windows x64 (the MSVC `.CRT$XIU` fixup path is untested; `long` is 32-bit there, ~18 `(long)` casts, mostly fseek). Rebuild non-default configs (non-INTERNAL).
9. **Regenerating static fixups.** `gen_fixups.py` is one-shot. Anyone editing a static initializer that contains pointers must update its `CTR_P32_STATIC_FIXUP` block by hand. A `--check` mode would help.

## Open problems and risks

- **Fields that hold non-pointers.** The rewriter converted every pointer-typed field. Any that actually hold integers (like the three in rule 5) abort in `CtrPtr32_RangeError` when the value is large, or pass silently when it is within +-16. More may exist on paths not yet exercised.
- **Casts hide stride bugs.** `(T **)x` on a slot array compiles without warning. A grep for `\w+ \*\*)` casts in `game/` and `platform/` was clean at the time of writing.
- **Wrong-type decoding.** `P32_SET` takes any pointer and `P32_GET` casts to whatever is written, so mismatched pointee types are no longer caught.
- `include/psx/*` structs (TMD, TIM, EVCB) were left as native pointers; they are not retail layout.

## Headless testing

Run a scratch copy (binary, `config.ini`, `debug/`, `memcards/`, `mods/`, `assets` symlink) under `xvfb-run` and `bwrap` with `/dev/input` hidden, as in the `headless-ab-testing` memory. With no gamepad visible the keyboard starts as player 3, so press F4 twice to make it player 1. Debug builds boot slowly; this sequence reaches the hub: boot 12 s, `F4 F4`, wait 12, `Return`, wait 15, `Return`, wait 12, `c`, wait 5, `c`, wait 5, `c`, wait 40. Holding `c` for 9 s from the hub spawn drives into the Crash Cove warp pad.

## How to resume

```
cd ".../CTR Turbocharged/src/worktrees/64bit"
mkdir -p /tmp/ctr64-tools && cp tools/ctr64/* /tmp/ctr64-tools/
cmake -S . -B /tmp/b64 -DCTR_NATIVE_64BIT=ON -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_C_FLAGS="-ferror-limit=0 -fno-color-diagnostics"
cmake -S . -B /tmp/b32 -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCMAKE_C_FLAGS=-m32 -DCMAKE_EXE_LINKER_FLAGS=-m32
make -C /tmp/b64 -j && (cd /tmp/b64 && ctest)
make -C /tmp/b32 -j && (cd /tmp/b32 && ctest)   # 32-bit must stay green
# plus /tmp/b32sse for A/B (see "A/B testing")
```

Pitfalls: do not use bare `git checkout game/` to "reset" while uncommitted work exists; the stash stack is shared with other sessions, so avoid it; `/tmp/p32` is an unrelated file from another session and must be left alone.
