# 64-bit / ARM64 port: handoff

Branch `64bit`, worktree `src/worktrees/64bit`, based on `turbocharged` @ 8cbfb7797.
Written 2026-10-03. Nothing is pushed. The working tree was clean when this was written, apart from the new `tools/ctr64/` scripts and this file.

## Goal

Build and run the game on 64-bit targets (x86-64 Linux first, then ARM64, macOS, Windows x64) using the most portable method available. That rules out low-memory tricks (`MAP_32BIT`, `-no-pie`, `__ptr32`) and anything tied to clang or MSVC. The 32-bit build must keep working unchanged.

## Design

Retail-shaped structs keep their 4-byte pointer fields. On 64-bit builds each such field is a `CtrPtr32` handle holding a signed 32-bit offset from `gCtrPtr32Anchor`. Handle 0 is NULL. Because layouts are identical, hex offsets, JitPool item sizes, the 501 `CTR_STATIC_ASSERT`s and in-place file patching (`LOAD_RunPtrMap`) all keep working.

Handles only work if every pointee is within +-2 GiB of the anchor. That holds for statics, string literals, functions and the mempack arena (a static array). It does NOT hold for `malloc`ed buffers, stack addresses or SDL memory, so those cannot be stored in game structs (see "Open problems").

API, all in `include/ctr_ptr32.h` (identity on 32-bit):

| Macro | Meaning |
|---|---|
| `P32(T)` | declare a pointer field of type `T` (`T` is the full pointer type, e.g. `struct Model *`) |
| `P32_FNPTR(ret, name, (args))` | declare a raw function-pointer field |
| `P32_GET(T, lv)` | read `lv` as a pointer of type `T` |
| `P32_SET(lv, v)` | store pointer `v` into `lv` (statement use only; its value is not a pointer) |
| `P32_ENC(v)` / `P32_DEC(T, h)` | convert a raw pointer to/from a handle kept in an int |
| `P32_DEFER(e)` | static-initializer value: `(e)` on 32-bit, `0` on 64-bit |
| `CTR_P32_STATIC_FIXUP(name)` | constructor that stores deferred static pointers at startup |
| `CTR_P32_MUTABLE` | `const` on 32-bit, empty on 64-bit (for data patched at startup) |

`gCtrPtr32Anchor` and `CtrPtr32_RangeError` live in `platform/native_memory.c`. The range check aborts if a pointer is outside +-2 GiB.

## Commits on this branch

1. `8e110e4b7` adds the `CTR_NATIVE_64BIT` CMake option, relaxes the `sizeof(void *) == 4` guards, and adds the `linux-x64-debug` preset.
2. `380bbb142` converts about 440 pointer fields in `include/` to `P32` and rewrites about 13k access sites.
3. `687779622` moves 775 static pointer initializers into startup fixups.

After each commit the 32-bit build (`/tmp/b32`, gcc `-m32`) compiled and `ctest` passed 5/5. The 32-bit **game has not been run** since the conversion. It should be identical, but verify.

## What was converted, and what was left alone

Converted: every pointer field in structs under `include/`, except `include/psx/`, `include/psn00bsdk/`, `include/platform/`, `include/platform.h`. Native-only structs defined in `.c` files (RenderBucket contexts, `NativeDrawLevel`, audio, CD, etc.) are intentionally left as real pointers.

Not touched by the rewrite: `platform/native_checkpoint.c` (needs hand work, see below).

## Tools (`tools/ctr64/`)

They need libclang (`python3 -c "import clang.cindex"` works on this machine) and a configured 64-bit build dir with `compile_commands.json`. They hard-code `/tmp/b64` and the worktree path in `common.py`.

- `common.py` parses `main.c` (the project is a unity build, so one TU covers almost everything).
- `rewrite.py` is pass 1: field declarations plus access sites. It is already applied and committed; re-running it on the current tree would double-wrap. It is kept as a record.
- `rewrite2.py` is pass 2: remaining sites, mainly inside macro arguments. Dry run by default (`--apply` to edit). Writes `/tmp/ctr64-tools/report2.txt`. **Not applied yet.** Last dry run: 350 edits in 56 files plus a manual list (see below).
- `gen_fixups.py` is the static-initializer pass (applied). It reads the clang JSON AST. Quirk: array elements are under `array_filler[1:]` when a filler exists.
- `b64.sh [n] [m]` builds `/tmp/b64` and summarises the errors. `/tmp/b64` is configured with `-DCTR_NATIVE_64BIT=ON -DCMAKE_C_COMPILER=clang -DCMAKE_C_FLAGS="-ferror-limit=0 -fno-color-diagnostics" -DBUILD_TESTING=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`.
- `step2.py`, `manual1.py`, `staticvars.py` are small one-offs. Do NOT re-run `manual1.py`: it duplicates edits already committed.

`/tmp` does not survive a reboot. The scripts are copies; the originals are in `/tmp/ctr64-tools`.

## Current state of the 64-bit build

`/tmp/ctr64-tools/b64.sh` reports **426 errors**, all from sites the first pass could not convert:

- about 137 `member reference type 'CtrPtr32' is not a pointer` and 42 `operand ... where arithmetic or pointer type is required`: unconverted accesses, mostly inside macro arguments (`ST1_GETPOINTERS(sdata->gGT->...)`, `gte_SetTransVector(d->instSelf->matrix.t)`, `CTR_FRAME_STEP(step, sdata->gGT->timer)`).
- about 77 in `platform/native_checkpoint.c`.
- the rest are address-of uses, array decays, int-typed pointer carriers and a few size asserts (below).

## Next steps, in order

1. **Apply `rewrite2.py`.** Run it dry first and check `report2.txt`. It uses spelling locations to rewrite macro-argument sites. Spot-check a few diffs (`game/CAM.c:483`, `game/223.c:277`). Rebuild 32-bit and 64-bit, then commit.
2. **Hand-fix the manual list** (`report.txt` from pass 1 and `report2.txt`):
   - About 40 address-of sites (`&x->ptrField`) passed as `void **` or `T **`, e.g. `LOAD_Assets.c` `fileBase`, `LOAD_TenStages.c` podium models, `PROC.c` thread links, `Particle.c` list heads, `HOWL_Channel.c`. Use a local `T *tmp`, call, then `P32_SET`, or change the callee.
   - About 40 array-decay uses (`R233.introCutsceneOpcodes`, `gGT->ptrIcons`, `sdata_static.quadBlocksRendered`, `visOVertList`): the array of `CtrPtr32` is passed where `T **` is expected.
   - About 490 "macro body" sites: expected to be a few macro definitions that mention a field (`OVR233_GARAGE_INITIALIZER` in `game/233/D233.c`, and anything in headers). Fix the macro bodies by hand. Many reported entries (`uiOT`, `instSelf`, `level1`) point at macros defined outside the converted headers; check each.
   - `D233.c` garage initializer: its pointer leaves are inside a macro body, so `gen_fixups.py` skipped them (2 manual). Handle `s_gGarageInitialState` and `gGarage` by hand.
   - 1 chained assignment: `tests/native_physics_test.c:563`.
3. **Int-typed pointer carriers.** The decomp stores addresses in `int`/`u32` and casts back (`(int)&x`, `(u32)ptr`, about 290 sign-extending round trips such as `INSTANCE.c:234`, `HOWL_Load.c:144`). Convert each to `P32_ENC`/`P32_DEC` (stored in a struct) or `uintptr_t` (local arithmetic). Known sites: `PushBuffer.c` `(int)&field + off`, `MainFrame_RenderFrame.c` passing `(u32)&...`, `zGlobal_DATA.c:3372` `voiceSetPtr`, `MEMPACK.c` `(u32)` subtractions, `GhostReplay.c:179`, `CS_Credits.c:256`, `LOAD_Assets.c:627`. The 64-bit warning list (`-Wpointer-to-int-cast`, `-Wint-to-pointer-cast`) gives the full set once the hard errors are gone.
4. **Remaining static asserts**: `RenderBucketEntry`, `VehGroundShadowEntry` (these are native structs in `.c` files that had pointers; decide whether they should be converted or their asserts made 64-bit aware), `DriverModelExtraSlot == sizeof(void *)` (`regionsEXE.h:97`), `offsetof(struct Data, currSlot)` (`regionsEXE.h:2792`), `Driver.funcPtrs`, `NavHeader`, `CameraDC`. Most of the earlier 361-assert baseline disappeared with the P32 conversion; re-check what is left.
5. **Fix `*(void **)dest = file` style hidden 8-byte writes**: `LOAD_File.c:187,280`, `LOAD_TenStages.c:613-633`. Also grep `memcpy`/`sizeof(void *)` that touch converted fields.
6. **Checkpoint code (`platform/native_checkpoint.c`, `native_checkpoint_file.c`)**: it stores pointers as `u32`, rejects values above 4 GB (`:122-133`) and stores a code address (`:2039`). With handles it can serialise the raw handle values, which are stable across runs. `struct PlatformMempackArena` (`include/platform.h`) holds three `void *` that go into the checkpoint header, so checkpoints will not move between 32- and 64-bit builds; make those `u32` handles if cross-build A/B is wanted.
7. **Allocations that must be inside the image** (see Open problems): `LOAD_Assets.c:174`, `native_custom_racer.c:837/854` (this one also runs `LOAD_RunPtrMap`), VRM buffers (`LOAD_Assets.c:882`, `:1745`), any `malloc` whose result is stored in a game struct.
8. **Get it to link and boot**, then run the headless recipe from the `headless-ab-testing` memory in a scratch copy of the game dir. `CtrPtr32_RangeError` will abort with the offending pointer, which is the main tool for finding anything missed.
9. **A/B against the 32-bit build** using Scroll Lock (frozen logic) plus identical inputs/replays (`docs/REPLAYS.md`) and compare frames and state. Silent truncation shows up as gameplay divergence, not crashes.
10. **Tests**: `tests/*` include `zGlobal_DATA.c`, so every error shows up there twice; they also need the fixup constructors, which live next to the data definitions and should run automatically.
11. **Other targets**: ARM64 Linux, macOS arm64 (check the +-2 GiB assumption holds for the Mach-O image), Windows x64 (MSVC `.CRT$XIU` fixup path is written but untested; `long` is 32-bit there, about 18 `(long)` casts, mostly harmless fseek). Web (wasm32) and Vita stay 32-bit and use the identity macros. Remember to rebuild the non-default configs (non-INTERNAL, Vita-conditional code): inactive `#if` regions were never parsed by the rewriter.

## Open problems and risks

- **Stack addresses stored in structs**: the stack is outside +-2 GiB. Either audit those stores or run the game loop on a thread whose stack is allocated from a static buffer.
- **`malloc`ed pointers**: same issue; small `brk` heap allocations may land near the image by luck on Linux, which makes bugs intermittent. Allocate from static pools instead.
- **Sentinel "pointers"** (small integers or `-1` cast to a pointer) do not survive `ctr_p32_enc`; they hit `CtrPtr32_RangeError` and must be handled.
- **Regenerating static fixups**: `gen_fixups.py` is one-shot. If someone edits a static initializer that contains pointers, they must update the generated `CTR_P32_STATIC_FIXUP` block by hand. A `--check` mode that diffs against the source would be worth adding. Fixups write into the data at startup, so anything that later re-copies pristine data must also copy pointers correctly (handles are plain `u32` in memory, so struct copies are fine).
- **`P32_GET` on hot paths** adds a branch and an add per access. Not measured yet.
- **Wrong-type decoding**: `P32_SET` takes any pointer, so a mismatched pointee type is no longer caught by the compiler.
- `include/psx/*` structs (TMD, TIM, EVCB) were left as native pointers; they are not retail layout but check nothing relies on their size.

## How to resume

```
cd "…/CTR Turbocharged/src/worktrees/64bit"
cp tools/ctr64/*.py tools/ctr64/b64.sh /tmp/ctr64-tools/   # if /tmp was wiped
cmake -S . -B /tmp/b64 -DCTR_NATIVE_64BIT=ON -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_C_FLAGS="-ferror-limit=0 -fno-color-diagnostics"
cmake -S . -B /tmp/b32 -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCMAKE_C_FLAGS=-m32 -DCMAKE_EXE_LINKER_FLAGS=-m32
/tmp/ctr64-tools/b64.sh          # error summary for the 64-bit build
make -C /tmp/b32 && (cd /tmp/b32 && ctest)   # 32-bit must stay green
python3 /tmp/ctr64-tools/rewrite2.py         # dry run of pass 2
```

Pitfalls hit so far: do not use bare `git checkout game/` to "reset" while uncommitted work exists (it wiped uncommitted edits once); the stash stack is shared with other sessions, so avoid it; `/tmp/p32` is an unrelated file from another session and must be left alone.
