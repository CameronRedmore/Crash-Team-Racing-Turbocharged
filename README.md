# Crash Team Racing: Turbocharged
<img src="screenshots/game1.jpg"></img><br>
Crash Team Racing: Turbocharged is a fork of [Crash Team Racing: High Octane](https://github.com/Rinnegatamante/Crash-Team-Racing-High-Octane), a sourceport for PSVita, PC (Windows) and Web Browser based on the [ctr-native](https://github.com/CTR-tools/ctr-native) project. Turbocharged extends High Octane with additional rendering, precision, and quality-of-life options.

This fork is focused on PC operating systems, especially Windows and Linux. The PSVita and Web Browser builds are not supported in Turbocharged. See the [Roadmap](#roadmap) for planned platforms and features.

## Trailer

[![Watch the Crash Team Racing: Turbocharged trailer](screenshots/trailer.png)](https://youtu.be/e2bI3uvGHlI)

[Watch the trailer on YouTube](https://youtu.be/e2bI3uvGHlI).

## First-time setup (Windows / Linux)

### 1. Download and extract the latest release

Open [GitHub Releases](https://github.com/CameronRedmore/Crash-Team-Racing-Turbocharged/releases) and choose the newest Turbocharged release, including a beta if that is the latest available. Under **Assets**, download the archive for your operating system:

- **Windows:** `ctr-turbocharged-<version>-windows-x86.zip`
- **Linux:** `ctr-turbocharged-<version>-linux-x86.AppImage` or `ctr-turbocharged-<version>-linux-x86.tar.gz`

Choose a game download rather than GitHub's **Source code** downloads. For ZIP/tar.gz archives, extract the entire archive into a folder you can write to, keeping the executable and its `assets` folder together. Settings, saves, and caches are stored in this folder.

**Linux AppImage:** make the downloaded file executable (`chmod +x ctr-turbocharged-<version>-linux-x86.AppImage`) and launch it. The first launch creates the game data folder at `~/.local/share/ctr-turbocharged` (or `$XDG_DATA_HOME/ctr-turbocharged` if configured). For the steps below, use that folder as `Turbocharged/`. The disc setup popup copies your selected image into its `assets/ctr-u.bin`; the optional font goes in `assets/fonts/crash-a-like.ttf`. Settings and saves live there too, so replacing the AppImage keeps your progress.

To use an existing installation or another writable data folder, run `CTR_TURBOCHARGED_DATA_DIR="/absolute/path/to/Turbocharged" ./ctr-turbocharged-<version>-linux-x86.AppImage`. If FUSE is unavailable, add `--appimage-extract-and-run` immediately after the AppImage filename.

The PC builds are 32-bit and require an OpenGL 3.3 capable GPU/driver. On Linux, including with the AppImage, you also need 32-bit glibc, graphics, windowing, and audio runtime libraries; see the release's `README.txt` for requirements. Use your distribution's packages for these dependencies.

### 2. Add your game disc image

Game data is not included. Dump your own **NTSC-U (US) retail Crash Team Racing** disc, then launch Turbocharged. If the game cannot load its required assets, a **disc setup popup** lets you choose the raw BIN file. It checks the sector layout, NTSC-U executable/boot configuration, required game files and audio manifest, and reads every sector for structural/read errors. The checks validate compatibility and structure, rather than comparing the image against a retail checksum.

The image is copied automatically to **`assets/ctr-u.bin`** in the installation/data folder; your original is kept. A progress window stays open during validation and copying. Invalid images show a reason and can be retried. Replacing an existing image requires confirmation; the copied file is checked before it replaces the existing image.

You can also name the raw BIN image **`ctr-u.bin`** and place it manually in the game's **`assets`** folder:

```text
Turbocharged/
  Crash Team Racing - Turbocharged.exe   (Windows)
  ctr_native                            (Linux)
  assets/
    ctr-u.bin
    dualshock.png
    fonts/
      FuzzyBubbles-Bold.ttf
      FuzzyBubbles-OFL.txt
```

ZIP/tar.gz downloads contain the executable for your chosen platform; keep the other supplied files in place. The AppImage contains its executable internally and copies the supplied fonts and overlay into your data folder on launch.

The disc image must use **raw MODE2/2352 sectors**, with the data track starting at byte 0. A standard **2048-byte `.iso` is not supported**: renaming it to `.bin` will not restore the XA audio and STR video data. If your dump has a BIN/CUE pair, use the raw BIN file; the game does not load the CUE. You do not need to extract `BIGFILE.BIG`, audio, or video files separately. On Linux, the file picker uses your desktop's file dialog service (XDG portals or Zenity); if unavailable, use manual placement.

### 3. Add the optional Crash-a-Like font

**Luckiest Guy from Google Fonts is bundled**, so no extra font download is needed. Choose **Use Luckiest Guy** in first-time setup, or select **Options > Interface > Font > LUCKIEST GUY**. The Turbocharged preset uses Crash-a-Like when it is installed, otherwise Luckiest Guy. ORIGINAL remains available, and PS1/Vanilla+ presets use it. Existing saved font selections keep their meaning.

First-time setup offers an **optional font step** after disc import, or when you first launch with the disc already installed. If you already have Crash-a-Like, choose **Choose Crash-a-Like** and select its extracted TTF. Setup checks the TrueType header and required font tables, then copies it to **`assets/fonts/crash-a-like.ttf`**, keeping your original. Existing fonts require confirmation before replacement; a valid installed font skips this step.

To download Crash-a-Like instead, choose **Open README** for these instructions. Download [Crash-a-Like by Aryel Filipe from DaFont](https://www.dafont.com/crash-a-like.font) and extract the font archive. You can also copy **`crash-a-like.ttf`** manually into **`assets/fonts/`**. Create that folder if necessary. The final path must be:

```text
Turbocharged/assets/fonts/crash-a-like.ttf
```

Copy the TTF itself, rather than the ZIP or an extra containing folder. It does not need to be installed as a system font. Start the game and select **Options > Interface > Font > CRASH-A-LIKE**. If the game was already running when you added the file, restart it first.

Crash-a-Like is optional and is not redistributed by this project. **ORIGINAL** uses the game's existing font. The Fuzzy Bubbles font used for the TURBOCHARGED title lettering is already included in the release. Both bundled fonts are unmodified and ship with full licences and attribution: Luckiest Guy under Apache 2.0 and Fuzzy Bubbles under SIL OFL 1.1. See [bundled font notices](assets/fonts/README.md).

### 4. Launch and configure

- **Windows:** run `Crash Team Racing - Turbocharged.exe` from the extracted folder.
- **Linux archive:** open a terminal in the extracted folder and run `./ctr_native`. If its executable permission was lost during extraction, run `chmod +x ctr_native` first.
- **Linux AppImage:** launch the executable `.AppImage` file; its setup popup can import the disc image on first launch.

Use **Options** to configure controls, graphics, and your preferred frame rate. F11 switches between windowed and borderless fullscreen. The game creates `config.ini` for settings and `memcards/` for saves; keep the installation folder writable and back up your saves before replacing an older installation.

PAL voiceovers and custom characters are optional; their setup is described below. Python and xdelta3 are only needed for the optional conversion tools.

**Linux display server:** 32-bit builds do not get along with GPU drivers under native Wayland, so Turbocharged runs through X11 by default, using XWayland on Wayland desktops, and only falls back to Wayland if X11 is unavailable. A `SDL_VIDEODRIVER=wayland` setting in your environment is ignored. To really use native Wayland, also set `CTR_TURBOCHARGED_ALLOW_WAYLAND=1`. Other `SDL_VIDEODRIVER` values, such as `x11`, are respected.

**Linux audio:** the game tries PulseAudio, then PipeWire, then ALSA. Because the build is 32-bit, it can only use the 32-bit client libraries you have installed (for example `lib32-libpulse` or `lib32-pipewire`). If a driver opens but you hear nothing, such as ALSA on a PipeWire system without a 32-bit ALSA bridge, force one with `SDL_AUDIO_DRIVER`, for example `SDL_AUDIO_DRIVER=pipewire ./ctr_native`. In Steam, set the launch options to `SDL_AUDIO_DRIVER=pipewire %command%`. Try `pulseaudio` if that does not help. Running from a terminal prints the driver in use as `SDL audio stream opened: driver=...`.

If startup fails, check the exact `assets/ctr-u.bin` location and image format first. On Linux, run from a terminal and check `ldd ./ctr_native` for missing runtime libraries. When reporting a problem, include the release version, operating system, GPU/driver, and the game's `.log` file from the installation folder.

## Features

- True widescreen with no stretching.
- Internal resolution of the renderer bumped to 960x544.
- FXAA, MSAA or SSAA for anti-aliasing.
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
- Several vanilla game bugfixes (eg: PVS related glitches and Penta-Penguin wrong mask powerup HUD icon).
- Ghost Replay feature: Replay all your ghost datas as if you're seeing the run being played live with inputs viewer overlay.
- Increased ghost data limits: No more 7 ghosts globally, now there are 7 ghosts data slot per track.
- Stats viewer for characters in the character selection screen.
- [Upstream High Octane, disabled in Turbocharged] Online leaderboard for Time Trials and Relic Race results. It is switched off in Turbocharged builds, so nothing is uploaded or downloaded.
- Splitscreen support for up to 4 players local multiplayer.
- [Upstream High Octane, temporarily disabled in Turbocharged] Discord Rich Presence on Windows.
- Reverse tracks mode for Crash Cove, Roo's Tubes, Tiger Temple, Coco Park, Dragon Mines, Tiny Arena, Slide Coliseum and Turbo Track available in Time Trial and Relic Race mode.
- Relic Race mode available outside of Adventure mode and accessible with any character.
- Relic Race mode now has ghosts support.
- Custom characters support with pre-baked animations option.
- Personal bests tracking during Time Trials.
- Alternative Temple Tiger variant for Time Trial and Relic Race.
- Fully rebindable controls.
- And many many more...

### Turbocharged exclusive features

- [PC, Native 3D] Options > Display > Aspect Ratio selects 4:3, 16:9, 16:10, 21:9 or 32:9. Select Native 3D under Options > Graphics > Renderer first. Aspect Ratio defaults to 16:9 and is saved as `aspect_ratio=0..4` in the order above. The selected ratio fits inside the current window or fullscreen display with black bars when necessary. Projection preserves the vertical field of view, while fonts, HUD icons and minimaps adapt to the selected ratio. Aspect Ratio and Renderer can be changed from the main menu; both are locked in the pause menu. Classic keeps its existing 16:9 presentation. Ultrawide modes use broader level visibility and may need more CPU/GPU time.
- [PC] Options > Gameplay > Engine Selection adds a second step to the existing character menu: choose Balanced, Acceleration, Speed or Turning independently of your character. The setting can be enabled in-game; the engine choice is made at the next character selection. Penta's special PAL profile is available once Penta is unlocked. Off by default; saved as `engine_selection=0/1`. Engine choices last for the current session. Ghosts retain the selected profile; results with an engine different from the character's default are kept locally and excluded from online leaderboard uploads.
- [PC] Main Menu > Unlocks lists the base game unlock requirements and current status. Browse a scrolling reward list with Up/Down (or Left/Right); the selected reward’s status and wrapped requirements appear in a separate panel. Penta’s relic requirement is listed; N. Oxide appears when Additional Unlocks is enabled.
- [PC] Options > Gameplay > Additional Unlocks: earning a Gold (or Platinum) Relic on every Adventure track unlocks Penta Penguin; beating every N. Oxide ghost on the original Time Trial tracks unlocks N. Oxide. Oxide unlocks after completing the ghosts, and completed ghost progress is also recognized when the active Adventure profile loads or you enter a hub. On by default; saved as `additional_unlocks=0/1`.
- [PC] Options > Experimental > Kart Hue (Experimental) rotates the paint colour of human players' karts. Original keeps the retail colours; left/right adjusts the hue in 15-degree steps. Saved as `kart_hue`.
- New character, track, battle arena and scrapbook unlocks show a shared five-second popup with the reward name and a chime, including Additional Unlocks and Slide Coliseum's Adventure access. Multiple rewards queue; popups wait through loading, cutscenes, mask hints and active races. Existing rewards imported from saves do not trigger notifications.
- Selectable 30, 60, 90, 120, 144, and 240 FPS in the Options menu, with scaled game timing, interpolated model animation, and frame-rate-correct menu, UI, and effect animations. Higher rates need sufficient CPU/GPU performance; pacing retains the original NTSC clock (approximately 59.82 FPS for the 60 FPS setting).
- Additional FPS options and high precision rendering, including sub-pixel vertex precision and perspective-correct textures.
- High precision physics and collision options, level-of-detail overrides, and optional depth buffering.
- Adventure Mode character switching and automatic progress saving, with a CONTINUE option at the top of the Adventure menu that loads your last autosave directly.
- The in-game pause menu opens the options menu, with settings that are unsafe mid-race locked, plus a Gamepad / Vibration row.
- Options > Skip Mask Hints skips automatic Aku Aku / Uka Uka hint cutscenes in Adventure Mode while still unlocking each triggered hint in the Hints menu. Off by default; the setting is saved as `skip_mask_hints` in `config.ini`.
- [PC] HD Pause (Options > Interface): the pause backdrop is captured at full resolution instead of the original 4bpp tile grid. Off, Posterised or Smooth; the default is Smooth.
- An "all characters" unlock toggle in the cheats menu.
- [PC] PGXP option (Options > Graphics): sub-pixel vertex precision removes polygon wobble, and the Perspective setting also removes affine texture warping.
- [PC] Options > Gameplay groups independent Original/Smoothed settings for player physics, AI, collisions, and steering. Smoothed uses floating-point calculations; Original remains the default for player physics, AI and steering, while collisions default to Smoothed. Delete toggles player physics in development builds.
- [PC] Detail option (Options > Graphics): Maximum keeps tracks and models at their highest level of detail at every distance (sharpest textures, no low-poly models).
- [PC] Modern HUD Icons (Options > Interface): native vector cross, circle, square and triangle buttons, menu navigation arrows, and shaded race countdown lights. Independent of the Font and Modern Minimap settings. Off by default; saved as `modern_hud_icons=0/1`.
- [PC] Modern Minimap (Options > Interface): collision-derived minimap backgrounds and native vector markers, with persistent caching, smooth outlines, crossing edges and matching track select previews. Maps are prepared when enabled. Off by default; saved as `modern_minimap=0/1`.
- [PC] Depth Buffer (Options > Graphics): optional per-pixel depth testing corrects overlapping world polygons. Works with every PGXP setting; Off retains original polygon ordering; the default is On.
- [PC] Anti-aliasing option (Options > Graphics, cycle with left/right): Off, FXAA, MSAA 2x/4x/8x, or SSAA 2x/4x. MSAA smooths polygon edges at low cost; SSAA renders at 2x or 4x the pixel count and box-filters down, which also smooths texture and sub-pixel detail. FXAA remains the default; MSAA sample counts are capped at the GPU's limit.
- [PC] Resolution (Options > Graphics): PS1 renders at the original line count and horizontal pixel density (512x216 at 4:3, wider for wider aspects) and upscales with sharp bilinear filtering; HD renders at the window's resolution. Saved as `ps1_resolution`; the PS1 preset turns it on.

## Online leaderboard

The online leaderboard inherited from High Octane is disabled in Turbocharged. Time Trial and Relic Race results are only stored locally, and the game makes no network requests for it. It can be compiled back in by setting `CTR_NATIVE_HAS_LEADERBOARD` to 1 in `include/macros.h`, but that talks to the upstream author's server and is unsupported here.

## Tested Devices

These setups are confirmed working. Anything not listed has not been confirmed either way.

| OS | CPU | GPU / driver | Display | Build |
| --- | --- | --- | --- | --- |
| Arch Linux (CachyOS, kernel 7.2), KDE Plasma on Wayland, running through XWayland | AMD Ryzen 9 9950X3D | NVIDIA GeForce RTX 5090, driver 615.71.09 (OpenGL 4.6) | 16:9 | Linux 32-bit |

No Windows machine has been confirmed yet, so Windows reports are especially welcome.

### Help with testing

Please share what you find on your own hardware, whether it works or not. Open a [GitHub issue](https://github.com/CameronRedmore/Crash-Team-Racing-Turbocharged/issues) with your operating system and desktop/display server, CPU, GPU and driver version, which build you used (Windows zip, Linux tarball or AppImage), your renderer, aspect ratio and frame-rate settings, what worked and what didn't, and the `.log` file from the installation folder. Reports of working setups help as much as bug reports, and confirmed setups will be added to this table.

## Known Issues

- 32-bit libraries can cause issues with Wayland on some GPU drivers. Turbocharged therefore runs through X11 (XWayland) by default; see the Linux display server note above.
- Hue rotate (Options > Experimental > Kart Hue) can affect incorrect parts of some characters.
- Cross-platform ghosts, replays and saves are currently untested and may not work. Sharing them between different builds (for example 32-bit and 64-bit, x86 and ARM, or Windows and Linux) has not been tested. A ghost or replay may desync, and a save may fail to load. Floating-point behaviour can differ between platforms, which is a known risk for ghosts and replays. Keep backups of your saves and ghosts before moving them to another build.

### Reporting bugs

Please report bugs through [GitHub Issues](https://github.com/CameronRedmore/Crash-Team-Racing-Turbocharged/issues) using the bug-report form. Include the release version, operating system, GPU/driver, relevant settings, steps to reproduce, and the game's `.log` file from the installation folder. See [CONTRIBUTING.md](CONTRIBUTING.md) for details.

## Roadmap

Planned, in no particular order and with no promised dates:

- 64-bit build
- ARM build
- Android build
- macOS build (potentially)
- Re-add online functionality
- Re-instate the Web Browser build

## Special controls bindings

- [PC] F11 is a shortcut to swap between Windowed and Fullscreen Borderless mode.

## How to Install

Follow [First-time setup (Windows / Linux)](#first-time-setup-windows--linux) above for release downloads, disc image placement, the optional font, and launch instructions.

## How to use PAL voiceovers

- Install Python 3.11 or higher ([https://www.python.org/downloads/](https://www.python.org/downloads/)).
- Download [this script](https://github.com/CameronRedmore/Crash-Team-Racing-Turbocharged/raw/refs/heads/turbocharged/tools/extract_pal_voices.py) by right-clicking the link and selecting "Save link as..." or, if the script opens in the browser, "Save page as...".
- Place your PAL Crash Team Racing `.bin` dump in the same folder as the script.
- Open a command prompt in that folder by typing `cmd` in the File Explorer address bar and pressing Enter.
- Run `python extract_pal_voices.py YOUR_DUMP_NAME.bin`.
- When extraction is complete, place the generated `pal-voices` folder:
  - in the `mods` folder next to the CTR: Turbocharged executable, so that the final path is `mods/pal-voices`.

## How to add new custom characters to the game

- Install Python 3.11 or higher ([https://www.python.org/downloads/](https://www.python.org/downloads/)).
- Download [this script](https://github.com/CameronRedmore/Crash-Team-Racing-Turbocharged/raw/refs/heads/turbocharged/tools/import_custom_racer.py) by right-clicking the link and selecting "Save link as..." or, if the script opens in the browser, "Save page as...".
- Install xdelta3 separately using your package manager or download it from the [official releases](https://github.com/jmacd/xdelta/releases). Put it on your `PATH` or place the executable in the same folder as the script (`xdelta3.exe` on Windows, `xdelta3` on Linux). It is not bundled with Turbocharged.
Download a custom character in the form of an `.xdelta` patch and place it in the same folder as the script.
- Place your NTSC-U Crash Team Racing `.bin` dump in the same folder as the script.
- Open a command prompt in that folder by typing `cmd` in the File Explorer address bar and pressing Enter.
- Run `python import_custom_racer.py YOUR_DUMP_NAME.bin YOUR_PATCH.xdelta YOUR_CHARACTER.ctrr`.
- If xdelta3 is installed elsewhere, add `--xdelta3 "PATH_TO_XDELTA3"` to the command.
- If the patch replaces multiple characters, the importer automatically generates one `.ctrr` per changed racer, adding the original character name to the requested output filename (for example `YOUR_CHARACTER_crash.ctrr`, `YOUR_CHARACTER_cortex.ctrr`, etc.).
- *NOTE*: For static custom models, the script can automatically retarget and bake the animations of the original character being replaced onto the custom model. To enable this, run `python import_custom_racer.py YOUR_DUMP_NAME.bin YOUR_PATCH.xdelta YOUR_CHARACTER.ctrr --template-animations`. Models that already contain animations will not be overwritten.
- When conversion is complete, place the generated `.ctrr` file or files:
  - in the `mods/customracers` folder next to the CTR: Turbocharged executable, so that the final path is `mods/customracers/YOUR_CHARACTER.ctrr`.

## Changelog

Entries below Turbocharged are upstream High Octane history and are kept for reference.

### Turbocharged 2026.10.0 (first public release)

Changes relative to Crash Team Racing: High Octane v1.4.1, which this fork is based on.

- **Renderer:** new depth-first Native 3D renderer with a per-pixel depth buffer, static GPU level geometry, mosaic textures, full-dynamic LOD with a forced Maximum detail mode, and BSP frustum pruning. PGXP sub-pixel vertex precision and perspective-correct texturing. FXAA, MSAA 2x/4x/8x and SSAA 2x/4x anti-aliasing, and a PS1 resolution mode.
- **Display:** selectable aspect ratios (4:3, 16:9, 16:10, 21:9, 32:9) with correct projection, HUD, font and minimap handling; optional HD pause backdrop; modern HUD icons and a modern minimap.
- **Frame rate and physics:** frame rates up to 240 FPS with animation, particle, timer and surface-force timing scaled to match, plus independent Original/Smoothed floating-point modes for player physics, AI, collisions and steering.
- **Gameplay options:** engine selection independent of character, an Unlocks menu, Additional Unlocks (Penta Penguin and N. Oxide), cheats menu, CONTINUE quick-load for Adventure mode, native auto-save, remaining relic targets on the race clock, and an experimental Kart Hue option.
- **Interface:** reorganised options menu with presets, available from the pause menu; TrueType font support with Luckiest Guy bundled and optional Crash-a-Like.
- **Setup:** first-run disc image picker that validates and copies your NTSC-U BIN, and Linux tarball and AppImage packages with checksums and third-party notices.
- **Saves:** memory card and auto-save writes are atomic and keep a backup, with automatic recovery from a corrupt save.
- **Removed or disabled:** the online leaderboard and Discord Rich Presence are disabled, and the Web Browser build is unsupported for now.

### v.1.4.1

- Added multi characters mod support to the custom characters importer.

### v.1.4

- [PC Only] Added an option to disable dithering.
- Added controls rebinding support in the Options menu.
- Fixed a bug causing main menu to get stale texts after AdHoc matches.
- Fixed a bug causing the mask grab animation to break in Hot Air Skywat under certain circumstances.
- Added the possibility to locally save ghosts from the Online Leaderboard.
- Added the possibility to challenge ghosts from the Ghost Replay end screen.
- Made so that the powerslide bar changes color dynamically instead of being only two distinct sections.
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

- Added an overlay when watching ghosts in Ghost Replay that shows the inputs the player used in realtime.
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
- Created a PC port (Windows) of CTR: High Octane. Has FXAA, Borderless window mode and Discord Rich Presence support.
- Added possibility to link online accounts for the Online Leaderboard.

### v.1.2

- Fixed N. Oxide portrait slideing in/out from the left instead of from the bottom in the Character Select screen.
- Fixed a bug causing big black glitched textures to show on screen under certain circumstances during singleplayer races.
- Optimized audio mixing and input handling code.
- Rewrote the whole renderer: now it's much closer to a GPU-style architecture. (Average GPU workload per frame went from 31ms to 16ms)
- Rewrote renderer pipeline so that now works in a multi-threaded fashion (backend/frontend approach). This reduces overall CPU workload per frame from 22 ms to 13ms.
- Added 60 FPS support. (Available in the Options menu)
- Added support for multiple controllers, allowing for local splitscreen games (up to 4 players).
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
- Optimized the missiles powerup rendering effect. Now there won't be anymore framedrops when missiles are on screen.
- Fixed a bug in vanilla game that was causing Penta Penguin powerup HUD to show Uka-Uka instead of Aku-Aku.
- Fixed a bug causing the Uka-Uka/Aku-Aku powerup to occasionally enter in stale setups, resulting in audio glitches (eg: powerup music playing permanently or playing when you were recovered from an out of track).
- Added ability to skip the intro from the very first frame of the SCEA copyright screen by pressing START.

## Building from source

Requires CMake 3.20 or newer and a **32-bit x86 target**: the game is a PS1 decompilation and still assumes 4-byte pointers. Only Windows and Linux are supported; the version comes from the `VERSION` file.

SDL and other dependencies are vendored in `externals/`. Use the presets in `CMakePresets.json`:

| Platform | Configure | Build |
| --- | --- | --- |
| Linux (gcc-multilib) | `cmake --preset linux-gcc-i686-release` | `cmake --build build-linux-gcc-i686-release` |
| Windows (MinGW32 / MSYS2) | `cmake --preset windows-mingw-i686-release` | `cmake --build build-mingw-i686-release` |
| Windows (Visual Studio 2022) | `cmake --preset windows-msvc-x86` | `cmake --build --preset windows-msvc-x86-release` |

Debug presets exist for Linux and MinGW (`...-debug`) and MSVC (`windows-msvc-x86-debug`). The Linux build needs 32-bit development packages for X11, OpenGL, ALSA/PulseAudio, udev and D-Bus; `build.sh` lists the Debian/Ubuntu names and the CI workflow `.github/workflows/build-native.yml` has the full list. On Windows, install the MSYS2 MinGW32 toolchain described in `build.bat`.

Shortcuts: `./build.sh` (Linux), `build.bat` (MinGW) and `build-msvc.bat` configure, build and run the tests. The Linux executable is `build/ctr_native`; Windows builds produce `Crash Team Racing - Turbocharged.exe`. Run it from a folder containing `assets/ctr-u.bin`, or let the first-run popup import your disc.

### Linux packaging

After building, run `./package-linux.sh` from the source directory to create both the tar.gz archive and the AppImage, plus SHA-256 checksums, in `dist/`. It uses `VERSION` by default; pass a version as the first argument to override it. For a preset build, use `BUILD_DIR=build-linux-gcc-i686-release ./package-linux.sh`.

AppImage packaging requires an x86_64 Linux host, `curl`, `readelf` (binutils), and the usual shell/archive tools. The script downloads checksum-pinned appimagetool 1.9.1 and the i686 runtime 20251108 into `.cache/appimage-tools` on first use; later builds can use those cached files offline. Set `APPIMAGE_TOOL_CACHE` to choose a different cache directory. Packaging runs without FUSE. The same allowlist used for the tarball supplies the AppImage's bundled assets; retail game data and the optional Crash-a-Like font are supplied by the player.

The script also downloads pinned runtime/libfuse/squashfuse sources and creates an `AppImage-runtime-source.tar.gz` archive with licences and rebuilding instructions. It is included inside the AppImage and uploaded separately by CI; publish it and its checksum alongside the AppImage. Source and binary distributions include [third-party notices](THIRD_PARTY_NOTICES.md) and full licence texts in `licenses/`.

For the tarball alone, use `./package.sh linux`. Linux CI builds and uploads both formats, runtime source, and their checksums, and checks the AppImage's version command without FUSE.

### Tests

Tests run under `ctest --test-dir <build>`. The renderer integration tests are
off by default; configure with `-DCTR_NATIVE_RENDERER_TESTS=ON` to build them.
With that option enabled, there are currently 32 tests: 25 without the `gpu`
label and 7 that need an OpenGL context. The headless font cache test skips
unless `assets/fonts/crash-a-like.ttf` is available; set
`CTR_NATIVE_FONT_ASSET_DIR` to use another assets directory.

```
ctest --test-dir build -LE gpu     # display-independent subset
ctest --test-dir build -L gpu      # requires an OpenGL context
```

See [CONTRIBUTING.md](CONTRIBUTING.md) for bug-report details, pull requests,
formatting, and asset/licence requirements.

### Formatting

The decompiled upstream tree is not clang-format clean and never will be, so
the standard is scoped to the diff against the `turbocharged` branch (the scripts compare with `origin/turbocharged`, so run `git fetch origin turbocharged` first): your lines must conform, but you are never
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

## Credits

- robin994 for helping testing splitscreen implementation.
- All the folks involved in ctr-native and the decompilation efforts of CTR.
