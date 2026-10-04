# Crash Team Racing: Turbocharged
<img src="screenshots/game1.jpg"></img><br>
Crash Team Racing: Turbocharged is a fork of [Crash Team Racing: High Octane](https://github.com/Rinnegatamante/Crash-Team-Racing-High-Octane), a sourceport for PSVita, PC (Windows) and Web Browser based on the [ctr-native](https://github.com/CTR-tools/ctr-native) project. Turbocharged extends High Octane with additional rendering, precision, and quality-of-life options.

This fork is focused on PC operating systems, especially Windows and Linux. PSVita and Web builds are not currently tested or a development priority. Support for ARM-based machines is planned for the future.

You can play it from Web Browser as well from this link: [Web Browser Build](https://rinnegatamante.github.io/Crash-Team-Racing-High-Octane/).

## Features

- True widescreen with no stretching.
- Internal resolution of the renderer bumped to 960x544.
- MSAA 4x (PSVita) / FXAA, MSAA or SSAA (PC) for anti-aliasing.
- Penta Penguin has its stats set to its PAL/NTSC-J counterpart (6/6/6).
- Playable Nitrous Oxide (Unlockable via the original Spyro 2 Demo cheatcode). (Credits: [Original mod](https://github.com/CTR-tools/CTR-ModSDK/tree/main/mods/Modules/OxideFix))
- Reserves Meter (Credits: [Original mod](https://github.com/CTR-tools/CTR-ModSDK/tree/main/mods/Modules/ReservesMeter))
- Customizable Cups (Credits: [Original mod](https://github.com/CTR-tools/CTR-ModSDK/tree/main/mods/Modules/CustomCups))
- Multilanguage support with (optional) PAL voiceovers support (Check "How to use PAL voiceovers").
- Super and Ultra Hard Difficulty modes for Arcade mode.
- USF will show as blue fire (similar to CTR: Nitro Fueled). (Credits: [Original mod](https://github.com/CTR-tools/CTR-ModSDK/tree/main/mods/Modules/BlueFire))
- Super turbopads are cyan to distinguish them from regular turbopads.
- Mirror mode option: Play any track specular.
- Boss Fight option: Challenge Adventure mode bossfights on any track.
- [PSVITA Only] AdHoc Netplay support for two PSVitas multiplayer without the need of a router.
- Several vanilla game bugfixes (eg: PVS related glitches and Penta-Penguin wrong mask powerup HUD icon).
- Ghost Replay feature: Replay all your ghost datas as if you're seeing the run being played live with inputs viewer overlay.
- Increased ghost data limits: No more 7 ghosts globally, now there are 7 ghosts data slot per track.
- Stats viewer for characters in the character selection screen.
- [PSVita/Windows/Web] Online leaderboard for Time Trials and Relic Race results (not available in Linux builds).
- Splitscreen support for up to 4 players local multiplayer for PC and PSTV users or PSVita users with MiniVitaTV.
- [Windows Only] Discord Rich Presence support when playing with Discord opened.
- Reverse tracks mode for Crash Cove, Roo's Tubes, Tiger Temple, Coco Park, Dragon Mines, Tiny Arena, Slide Coliseum and Turbo Track available in Time Trial and Relic Race mode.
- Relic Race mode available outside of Adventure mode and accessible with any character.
- Relic Race mode now has ghosts support.
- Custom characters support with pre-baked animations option.
- Personal bests tracking during Time Trials.
- Alternative Temple Tiger variant for Time Trial and Relic Race.
- Fully rebindable controls.
- And many many more...

### Turbocharged exclusive features

- [PC, Native 3D] Options > Display > Aspect Ratio selects 4:3, 16:9, 16:10, 21:9 or 32:9. Select Native 3D in the Renderer row first. Aspect Ratio defaults to 16:9 and is saved as `aspect_ratio=0..4` in the order above. The selected ratio fits inside the current window or fullscreen display with black bars when necessary. Projection preserves the vertical field of view, while fonts, HUD icons and minimaps adapt to the selected ratio. Aspect Ratio and Renderer can be changed from the main menu; both are locked in the pause menu. Classic keeps its existing 16:9 presentation. Ultrawide modes use broader level visibility and may need more CPU/GPU time. See [aspect-ratio implementation and checks](docs/ASPECT_RATIOS.md).
- [PC/Web] Options > Gameplay > Engine Selection adds a second step to the existing character menu: choose Balanced, Acceleration, Speed or Turning independently of your character. The setting can be enabled in-game; the engine choice is made at the next character selection. Penta's special PAL profile is available once Penta is unlocked. Off by default; saved as `engine_selection=0/1`. Engine choices last for the current session. Ghosts retain the selected profile; results with an engine different from the character's default are kept locally and excluded from online leaderboard uploads.
- [PC/Web] Main Menu > Unlocks lists the base game unlock requirements and current status. Browse a scrolling reward list with Up/Down (or Left/Right); the selected reward’s status and wrapped requirements appear in a separate panel. Penta’s relic requirement is listed; N. Oxide appears when Additional Unlocks is enabled.
- [PC/Web] Options > Gameplay > Additional Unlocks: earning a Gold (or Platinum) Relic on every Adventure track unlocks Penta Penguin; beating every N. Oxide ghost on the original Time Trial tracks unlocks N. Oxide. Oxide unlocks after completing the ghosts, and completed ghost progress is also recognized when the active Adventure profile loads or you enter a hub. On by default; saved as `additional_unlocks=0/1`.
- [PC/Web] Options > Experimental > Kart Hue (Experimental) rotates the paint colour of human players' karts. Original keeps the retail colours; left/right adjusts the hue in 15-degree steps. Saved as `kart_hue`.
- New character, track, battle arena and scrapbook unlocks show a shared five-second popup with the reward name and a chime, including Additional Unlocks and Slide Coliseum's Adventure access. Multiple rewards queue; popups wait through loading, cutscenes, mask hints and active races. Existing rewards imported from saves do not trigger notifications.
- Selectable 30, 60, 90, 120, 144, and 240 FPS in the Options menu, with scaled game timing, interpolated model animation, and frame-rate-correct menu, UI, and effect animations. Higher rates need sufficient CPU/GPU performance; pacing retains the original NTSC clock (approximately 59.82 FPS for the 60 FPS setting).
- Additional FPS options and high precision rendering, including sub-pixel vertex precision and perspective-correct textures.
- High precision physics and collision options, level-of-detail overrides, and optional depth buffering.
- Adventure Mode character switching and automatic progress saving, with a CONTINUE option at the top of the Adventure menu that loads your last autosave directly.
- The in-game pause menu opens the options menu, with settings that are unsafe mid-race locked, plus a Gamepad / Vibration row.
- Options > Skip Mask Hints skips automatic Aku Aku / Uka Uka hint cutscenes in Adventure Mode while still unlocking each triggered hint in the Hints menu. Off by default; the setting is saved as `skip_mask_hints` in `config.ini`.
- [PC/Web] HD Pause (Options > Interface): the pause backdrop is captured at full resolution instead of the original 4bpp tile grid. Off, Posterised or Smooth; the default is Smooth.
- An "all characters" unlock toggle in the cheats menu.
- [PC/Web] PGXP option (Options > Display): sub-pixel vertex precision removes polygon wobble, and the Perspective setting also removes affine texture warping.
- [PC/Web] Options > Gameplay groups independent Original/Smoothed settings for player physics, AI, collisions, and steering. Smoothed uses floating-point calculations; Original remains the default for player physics, AI and steering, while collisions default to Smoothed. Delete toggles player physics in development builds.
- [PC/Web] Detail option (Options > Display): Maximum keeps tracks and models at their highest level of detail at every distance (sharpest textures, no low-poly models).
- [PC/Web] Modern HUD Icons (Options > Interface): native vector cross, circle, square and triangle buttons, menu navigation arrows, and shaded race countdown lights. Independent of the Font and Modern Minimap settings. Off by default; saved as `modern_hud_icons=0/1`.
- [PC/Web] Modern Minimap (Options > Interface): collision-derived minimap backgrounds and native vector markers, with persistent caching, smooth outlines, crossing edges and matching track select previews. Maps are prepared when enabled. Off by default; saved as `modern_minimap=0/1`.
- [PC/Web] Depth Buffer (Options > Display): optional per-pixel depth testing corrects overlapping world polygons. Works with every PGXP setting; Off retains original polygon ordering; the default is On.
- [PC/Web] Anti-aliasing option (Options, cycle with left/right): Off, FXAA, MSAA 2x/4x/8x, or SSAA 2x/4x. MSAA smooths polygon edges at low cost; SSAA renders at 2x or 4x the pixel count and box-filters down, which also smooths texture and sub-pixel detail. FXAA remains the default; MSAA sample counts are capped at the GPU's limit.

## Online leaderboard

The full online leaderboard for Time Trials and Relic Races is available at this link: <a href="https://www.rinnegatamante.eu/ctr/leaderboard/">Online Leaderboard</a>.

## Known Issues

- [PSVita Only] The demo cutscene gets slightly de-synced during Oxide speech.

## Special controls bindings

- [PC/Web Browser] F11 is a shortcut to swap between Windowed and Fullscreen Borderless mode.

## How to Install (PSVita)

- Install the .vpk.
- Dump your US copy of `Crash Team Racing` for PS1 and place the bin file in `ux0:data/ctr/assets` named as `ctr-u.bin`.

## How to Install (PC)

- Dump your US copy of `Crash Team Racing` for PS1 and place the bin file in the `assets` folder named as `ctr-u.bin`.

## How to use PAL voiceovers

- Install Python 3.11 or higher ([https://www.python.org/downloads/](https://www.python.org/downloads/)).
- Download [this script](https://github.com/Rinnegatamante/Crash-Team-Racing-High-Octane/raw/refs/heads/vita/tools/extract_pal_voices.py) by right-clicking the link and selecting "Save link as..." or, if the script opens in the browser, "Save page as...".
- Place your PAL Crash Team Racing `.bin` dump in the same folder as the script.
- Open a command prompt in that folder by typing `cmd` in the File Explorer address bar and pressing Enter.
- Run `python extract_pal_voices.py YOUR_DUMP_NAME.bin`.
- When extraction is complete, place the generated `pal-voices` folder:
  - on PSVita: in `ux0:data/ctr/mods/`.
  - on PC: in the `mods` folder next to the CTR: Turbocharged executable, so that the final path is `mods/pal-voices`.

## How to add new custom characters to the game

- Install Python 3.11 or higher ([https://www.python.org/downloads/](https://www.python.org/downloads/)).
- Download [this script](https://github.com/Rinnegatamante/Crash-Team-Racing-High-Octane/raw/refs/heads/vita/tools/import_custom_racer.py) by right-clicking the link and selecting "Save link as..." or, if the script opens in the browser, "Save page as...".
- Download [xdelta3](https://github.com/Rinnegatamante/Crash-Team-Racing-High-Octane/raw/refs/heads/vita/tools/xdelta3.exe) and place it in the same folder as the script.
Download a custom character in the form of an `.xdelta` patch and place it in the same folder as the script.
- Place your NTSC-U Crash Team Racing `.bin` dump in the same folder as the script.
- Open a command prompt in that folder by typing `cmd` in the File Explorer address bar and pressing Enter.
- Run `python import_custom_racer.py YOUR_DUMP_NAME.bin YOUR_PATCH.xdelta YOUR_CHARACTER.ctrr`.
- If the patch replaces multiple characters, the importer automatically generates one `.ctrr` per changed racer, adding the original character name to the requested output filename (for example `YOUR_CHARACTER_crash.ctrr`, `YOUR_CHARACTER_cortex.ctrr`, etc.).
- *NOTE*: For static custom models, the script can automatically retarget and bake the animations of the original character being replaced onto the custom model. To enable this, run `python import_custom_racer.py YOUR_DUMP_NAME.bin YOUR_PATCH.xdelta YOUR_CHARACTER.ctrr --template-animations`. Models that already contain animations will not be overwritten.
- When conversion is complete, place the generated `.ctrr` file or files:
  - on PSVita: in `ux0:data/ctr/mods/customracers`.
  - on PC: in the `mods/customracers` folder next to the CTR: Turbocharged executable, so that the final path is `mods/customracers/YOUR_CHARACTER.ctrr`.

## How to set up Online functionalities on PC

Online functionality is only available in the Windows and Web builds, not on Linux.

In order to be able to auto submit your new records in Time Trial and Relic Race modes on PC, you need to set up an account first.
- Navigate to https://www.rinnegatamante.eu/ctr/account/ and create an account.
- Follow the instructions on screen to properly set up the connection on your PC setup.

## How to link PSVita and PC online accounts

- Navigate to https://www.rinnegatamante.eu/ctr/account/ and create an account.
- Follow the instructions on screen to properly link your PC account and your PSVita one.

## Changelog

### v.1.4.1

- Added multi characters mod support to the custom characters importer.

### v.1.4

- [PC Only] Added an option to disable dithering.
- [PSVita Only] Made so that controls scheme work as with a Dualshock controller (Aka true analogs support).
- Added controls rebinding support in the Options menu.
- Fixed a bug causing main menu to get stale texts after AdHoc matches.
- Fixed a bug causing the mask grab animation to break in Hot Air Skywat under certain circumstances.
- Added the possibility to locally save ghosts from the Online Leaderboard.
- Added the possibility to challenge ghosts from the Ghost Replay end screen.
- Made so that the powerslide bar changes color dynamically instead of being only two distinct sections.
- [PSVita Only] Improved performances in Tiger Temple by optimizing the flame spit by the statues.
- Added Tiger Temple Alternative in the map pool for Relic Race and Time Trial.
- [PC Only] Fixed a bug causing some controllers (eg. DualSense) to be detected as two controllers.
- Moved all the gameplay cheatcodes in a dedicated submenu in Options. Cheats will also now be saved and kept between game sessions.
- Made so that having any gameplay cheat enabled will disable the ability to save ghosts and records in Time Trial and Relic Race.
- Made so that in Arcade mode (Single and Cup) it's now possible to start races with arbitrary number of laps between 1 and 9.
- Fixed the speedometer having wrong scale.
- Fixed a bug causing some menu entries to be partially offscreen in certain languages.
- Fixed a lot of animations being at wrong speed when playing in 60 FPS.
- Adjusted physics at 60 FPS to more closely match 30 FPS mode. (This fixes the R in Hot Sky Airway during the CTR Challenge to be impossible to reach amongst other things)
- Fixed a vanilla game bug that was causing shadow casted from menu rects to cover other menu rects instead of being in background.
- Made so that enemy AIs in Arcade mode (Single and Cup) can pick unlockable characters, including N. Oxide.
- Added personal bests showcase (for both 1L and 3L) during Time Trials.
- Made so that current laps during a Time Trial change color based on how close you are to your personal best.
- Added custom characters support (Check the "How to add new custom characters to the game" for more info).
- Added a new option that allows to enable custom characters for enemy AIs in Arcade mode (Single and Cup).

### v.1.3.1

- Fixed a bug causing ghosts generated from PC to get desynced if right analog was used.
- Added a new option to change default camera between the close and far one.
- Added a new option to change default attachment between minimap and speedometer.
- Made so that analogs inputs are properly shown in the input viewer overlay.

### v.1.3

- Added a Vita overlay when watching ghosts in Ghost Replay that will show the inputs the player used in realtime.
- Fixed several animations playing at doubled speed when playing at 60 FPS.
- Fixed several sounds playing on both clients when they should be local during AdHoc netplay.
- Fixed "Final Lap" text not showing when playing in AdHoc.
- Fixed the Uka-Uka/Aku-Aku powerup causing constant desyncs resulting in heavy stutter during AdHoc netplay.
- Added "Vs" mode support to AdHoc netplay.
- Added support for PAL voiceovers (English, Italian, Spanish, German and Dutch) (Check the "How to use PAL voiceovers" in the README in order to set it up).
- Added possibility to play Relic Race gamemode outside of Adventure mode. (Available in the Time Trial submenu)
- Added ghosts support to Relic Race gamemode, including Ghost Replay support.
- Made so that super turbopads are now cyan to distinguish them from regular turbopads.
- Integrated Relic Mode into the Online Leaderboard system.
- Added Reverse variants for Crash Cove, Roo's Tubes, Tiger Temple, Coco Park, Dragon Mines, Tiny Arena, Slide Coliseum and Turbo Track. These are available in Time Trial and Relic Race.
- Created a PC port (Windows) of CTR: High Octane. It features everything available on the PSVita variant except for AdHoc mode. Has FXAA, Borderless window mode and Discord Rich Presence support. (In order to be able to compete with the Online Leaderboard, check the "How to set up Online functionalities on PC" in the README).
- Added possibility to link PSVita and PC online accounts for the Online Leaderboard (Check the "How to link PSVita and PC online accounts" section in the README).

### v.1.2

- Fixed N. Oxide portrait slideing in/out from the left instead of from the bottom in the Character Select screen.
- Fixed a bug causing big black glitched textures to show on screen under certain circumstances during singleplayer races.
- Optimized audio mixing and input handling code.
- Rewrote the whole renderer: now it's extremely closer to PSVita GPU architecture. (Average GPU workload per frame went from 31ms to 16ms)
- Rewrote renderer pipeline so that now works in a multi-threaded fashion (backend/frontend approach). This reduces overall CPU workload per frame from 22 ms to 13ms.
- Added 60 FPS support. (Available in the Options menu)
- Added support for multiple controllers on PSTV and PSVita with MiniVitaTV, allowing for local splitscreen games (up to 4 players).
- Made so that Sewer Speedway and Blizzard Bluffs environmental hazards are now deterministic. This also fixes broken ghosts on these specific tracks.
- Added an Online Leaderboard for Time Trial results. Your best scores will automatically be uploaded to it and you can watch ghosts of the top 5 scores worldwide.
- Fixed two different bugs both causing some tiles to be incorrectly clipped under certain circumstances.
- Made so that when an AdHoc connection is interrupted, the console will automatically return in Internet mode.
- Fixed a bug causing missiles used by enemy AIs to not be homing and instead always proceeding in a straight line.

### v.1.1

- Made so that the ghosts aren't limited anymore to 7 globally. You can now have 7 ghosts per track.
- Added a Ghost Replay feature that allows you to replay ghost data as if the race is running in single person. (Available only for ghost data generated from v.1.1 or higher)
- Refactored the main menu with submenus so that it's easier to navigate.
- Added a stats viewer in the character selection screen when playing in single player.
- Added Boss Fight mode. This mode allows you to play against the bosses from Adventure mode on any track.
- Optimized GPU workload by optimizing all the various shader variants used by the renderer: this improves overall framerate.
- Added AdHoc netplay: currently limited only to Arcade - Single Track mode, this allows for router-less 2 Vitas netplay.
- Optimized the missiles powerup rendering effect. Now there won't be anymore framedrops when missiles are on screen.
- Fixed a bug in vanilla game that was causing Penta Penguin powerup HUD to show Uka-Uka instead of Aku-Aku.
- Fixed a bug causing the Uka-Uka/Aku-Aku powerup to occasionally enter in stale setups, resulting in audio glitches (eg: powerup music playing permanently or playing when you were recovered from an out of track).
- Added ability to skip the intro from the very first frame of the SCEA copyright screen by pressing START.

## Building from source

Requires a 32-bit target: the game is a PS1 decompilation and still assumes
4-byte pointers. `CMakePresets.json` has working presets for all three
platforms — `cmake --preset linux-gcc-i686-release && cmake --build
build-linux-gcc-i686-release`.

Tests run under `ctest --test-dir <build>`. The renderer integration tests are
off by default because five of them need a GL context; configure with
`-DCTR_NATIVE_RENDERER_TESTS=ON` to build them. The 23 tests without a `gpu`
label run anywhere, with no display and no GPU:

```
ctest --test-dir build -LE gpu     # display-independent subset
ctest --test-dir build -L gpu      # the five that need a GL context
```

### Formatting

The decompiled upstream tree is not clang-format clean and never will be, so
the standard is scoped to the diff: your lines must conform, but you are never
forced to reindent code you did not write. Files this fork authored outright
are formatted in full.

The clang-format version is pinned in `.clang-format-version`, which
`check-format.sh`, the pre-commit hook and CI all read — output differs between
releases, so an unpinned checker disagrees with whatever you ran locally.

To get the format check on `git commit`:

```
git config core.hooksPath .githooks
```

This is not set automatically and is per-clone, so without it the gate is CI
only. `SKIP_FORMAT_HOOK=1` bypasses it for a single commit.

### Static analysis

`./check-tidy.sh` runs the repo's `.clang-tidy` checks over the files the fork
changed and summarises findings by check name. It is advisory in CI — the
config has been in the tree since upstream but nothing ran it, so the first
runs are about seeing what it reports. Pass `--strict` to make findings fail.

It needs a configured and built tree with a compile database, because the game
is a unity build: `game/game_unity.h` includes 261 `.c` files and `main.c`
includes that, so most changed files are not translation units and cannot be
handed to clang-tidy individually.

```
cmake --preset linux-gcc-i686-release && cmake --build build-linux-gcc-i686-release
./check-tidy.sh --build build-linux-gcc-i686-release
```

## vitaGL flags for compilation

`HAVE_SHADER_CACHE=1 NO_DEBUG=1 READBACKS_SPEEDHACK=1 CIRCULAR_POOL_SPEEDHACK=1`

## Credits

- Standard-Republic for the Livearea assets.
- robin994 for helping testing splitscreen implementation.
- All the folks involved in ctr-native and the decompilation efforts of CTR.
