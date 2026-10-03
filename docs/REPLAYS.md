# Replays

Use this for bug reports in internal builds.

## Quick State

- `F5`: save `debug/states/quick.ctrstates`
- `F8`: load `debug/states/quick.ctrstates`

## Record

```sh
build/ctr_native --record
```

Windows: use `build\Crash Team Racing - Turbocharged.exe` instead.

Normal saves live in `memcards/slot0`.

When recording starts, the CTR save files from `memcards/slot0` and `slot1` are copied to `memcard.seed`. The game records with a writable copy named `memcard.recording`, so saves and ghosts made while recording stay in the report.

To choose when recording starts:

```sh
build/ctr_native --record --toggle
```

- Press `F9` to start.
- Press `F10` to stop.

For more detailed reports:

```sh
build/ctr_native --record --detailed
```

You can combine both:

```sh
build/ctr_native --record --toggle --detailed
```

## Play Back

Use the command written in that folder's `metadata.txt`.

It looks like:

```sh
build/ctr_native --replay "debug/reports/20260605/ctr-123456/input.ctrreplay"
```

Playback creates a fresh writable `memcard.playback` from `memcard.seed` every run and does not touch your real saves.

If a developer asks you to bypass header identity checks:

```sh
build/ctr_native --replay "debug/reports/20260605/ctr-123456/input.ctrreplay" --replay-bypass-header
```

## Cross-Build A/B (developers)

Checkpoints store raw pointers on 32-bit builds and 4-byte handles on 64-bit builds, so a replay's bootstrap checkpoint only restores on the build that recorded it. To compare two builds, record from boot and replay without the checkpoint:

```sh
build32/ctr_native --record --ab-trace trace32.txt
build64/ctr_native --replay ".../input.ctrreplay" --replay-skip-bootstrap --replay-bypass-header --ab-trace trace64.txt
```

- `--replay-skip-bootstrap` needs a replay recorded with `--record` and without `--toggle`. Both runs then boot with VBlanks driven only by the game's own VSync calls, so boot ends in the same state.
- While recording or playing back, CD reads finish on the first callback pump after they are issued, so loads take the same number of frames every run.
- `--ab-trace` writes one line per frame: timers, RNG state, per-driver position, rotation, speed and item state, then per thread bucket `count:threadHash:instFlagsHash:matrixHash:modelIndexHash`. Pointer fields are not hashed. Diff two traces to find the first frame that differs.
- `modelIndex` hashes can differ between any two builds: some threads (`saveobj`) never set it and keep stale pool bytes.
- 32-bit x86 builds that use x87 floating point (gcc `-m32` default) drift from SSE builds in native float code such as smoothed collisions. Compare 64-bit builds against a 32-bit build made with `-msse2 -mfpmath=sse`.

`tools/ctr64/ab/` has the headless scripts used for this.
