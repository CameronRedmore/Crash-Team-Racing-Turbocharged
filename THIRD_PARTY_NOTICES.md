# Third-Party Notices

This project vendors third-party software and contains modified third-party
derivatives. Keep this file and the accompanying `licenses/` directory with
source and binary distributions of Crash Team Racing: Turbocharged. The
project's GPLv3 licence is in `LICENSE`; third-party files retain their own
licences and copyright notices.

## PsyCross / Psy-X

Source: <https://github.com/OpenDriver2/PsyCross>

PsyCross provided the starting point for parts of CTR Native's
Psy-Q-compatible PS1 hardware abstraction layer, including compatible GPU, GTE,
SPU, CD, and controller library interfaces. CTR Native now owns those headers
and native platform implementations in `include/` and `platform/` while
preserving Psy-Q-shaped APIs.

CTR Native contains modified/project-owned PsyCross derivatives in these
component areas:

- `include/psx/`: Psy-Q-compatible facade headers
- `include/platform/`: native GPU/renderer facade types and support headers
- `platform/`: native PS1 facade implementations, GTE/GPU/render support,
  platform shell code, and generated GL loader sources

Individual source files may carry narrower provenance notes where the original
PsyCross source path is useful during maintenance.

License: MIT

Copyright (c) 2020 REDRIVER2 Project

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## PSn00bSDK

Source: <https://github.com/Lameguy64/PSn00bSDK>

Path: `include/psn00bsdk`

CTR Native vendors a small PSn00bSDK header subset for PS1/Psy-Q-compatible
types, constants, and inline helpers used by the shared source. CTR Native does
not vendor or link `libpsn00b` into the native PC executable.

This notice applies to PSn00bSDK core files only. `mkpsxiso` and `dumpsxiso`
are separate GPLv2-or-later tools and are not distributed as part of CTR Native.

License: Mozilla Public License 2.0

The vendored header files retain their original copyright and license notices.
The full MPL 2.0 licence is included in `licenses/MPL-2.0.txt`.

## SDL3

Path: `externals/SDL`

SDL3 provides cross-platform host windowing, input, timing, and audio device
support for CTR Native.

Vendored version: 3.4.10 (`release-3.4.10`)

Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.

## stb_truetype

Path: `externals/stb/stb_truetype.h`

stb_truetype rasterises the optional TrueType HUD/menu fonts (Enhancements >
Font) into a signed-distance-field atlas.

Vendored version: 1.26

Copyright (c) 2017 Sean Barrett

Turbocharged distributes this component under the MIT option of its dual
licence. The full copyright notice and permission terms are included in
`licenses/stb-MIT.txt`, including in binary packages.

## Components included with SDL3

These components retain their upstream licences independently of SDL's zlib
licence. They can be compiled into the statically linked SDL library depending
on the platform and build configuration. Binary packages include these notices:

| Component | Vendored path | Licence text |
| --- | --- | --- |
| SDL3 | `externals/SDL` | `licenses/SDL-zlib.txt` |
| HIDAPI | `externals/SDL/src/hidapi` | `licenses/HIDAPI-BSD.txt` (BSD option selected) |
| stb_image 2.30 | `externals/SDL/src/video/stb_image.h` | `licenses/stb-MIT.txt` (MIT option selected) |
| yuv2rgb | `externals/SDL/src/video/yuv2rgb` | `licenses/yuv2rgb-BSD.txt` |
| EDID parser | `externals/SDL/src/video/x11/edid-parse.c` | `licenses/SDL-EDID-MIT.txt` |
| fdlibm maths routines | `externals/SDL/src/libm` | `licenses/SDL-fdlibm.txt` |

The `isinf` routines in SDL's maths subset also retain their public-domain
attributions to J.T. Conklin and Ulrich Drepper in their source headers.

## Fuzzy Bubbles

Path: `assets/fonts/FuzzyBubbles-Bold.ttf`

Source: <https://github.com/googlefonts/fuzzy-bubbles>

The 3D TURBOCHARGED lettering on the title screen is ray marched from a
distance field of this font (platform/native_title_logo.c).

Copyright 2005 The Fuzzy Bubbles Project Authors

Licensed under the SIL Open Font License, Version 1.1. The full licence text
ships beside the font as `assets/fonts/FuzzyBubbles-OFL.txt`.

The TTF is unmodified. The font and its full copyright/OFL text are included
in Windows ZIPs, Linux tarballs, and AppImages. Web builds embed the font and
licence and also provide a readable licence file under `assets/fonts/`.
The OFL font remains under the OFL, independently of the game's GPLv3 licence.

## Luckiest Guy

Path: `assets/fonts/LuckiestGuy-Regular.ttf`

Source: <https://github.com/google/fonts/tree/ac011ec057856b010986a55104323d63cada00f8/apache/luckiestguy>

Luckiest Guy Regular, version 1.001, is the bundled alternative menu/HUD font.
The Turbocharged preset uses it when the player has not installed Crash-a-Like.

Copyright (c) 2010 by Brian J. Bonislawsky DBA Astigmatic (AOETI).
All rights reserved. Available under the Apache 2.0 licence.

Luckiest Guy is a trademark of Astigmatic (AOETI).

The TTF is unmodified, including its embedded copyright, trademark, and licence
metadata. Full Apache 2.0 terms ship beside it as
`assets/fonts/LuckiestGuy-Apache-2.0.txt`; copyright, attribution, source, and
trademark notices are reproduced in `assets/fonts/LuckiestGuy-NOTICE.txt`.
The upstream font directory contains no separate NOTICE file.

Both files accompany the font in Windows ZIPs, Linux tarballs, AppImages, and
Web builds. The AppImage launcher also copies each bundled font's notices into
the player's data folder. Web deployments provide readable licence/notice files
under `assets/fonts/`. Keep these files with every redistributed font copy.
Provenance and redistribution notes for both fonts are in `assets/fonts/README.md`.

## AppImage runtime (Linux AppImage distributions)

The Linux AppImage embeds the i686 AppImage type 2 runtime, release `20251108`.
Its source and build instructions are available at:
<https://github.com/AppImage/type2-runtime/tree/20251108>.

The runtime is MIT licensed and statically links libfuse 3.15.0 (LGPL 2.1),
squashfuse 0.5.2 (BSD), musl (MIT and the notices in its COPYRIGHT file),
mimalloc (MIT), zstd (BSD option), and zlib (zlib licence). Full texts are
included in `licenses/appimage/`, inside the AppImage at
`usr/share/ctr-turbocharged/licenses/appimage/`. These libraries are part of
the embedded runtime; the game's host graphics/audio libraries are not bundled.

The AppImage includes `AppImage-runtime-source.tar.gz` beside its notices.
The same source archive is distributed separately as
`ctr-turbocharged-<version>-linux-x86.AppImage-runtime-source.tar.gz` with a
SHA-256 checksum. It contains the matching runtime sources and build scripts,
the libfuse source and runtime's patch, squashfuse source, licence texts, and
instructions for rebuilding/replacing the runtime with a modified libfuse.
Distribute this archive alongside the AppImage when publishing a release.

## Player-supplied files and build tools

Retail disc data, PAL voiceovers, custom racers, and the optional Crash-a-Like
font are supplied by the player and are not included in release packages.
The GPLv3 licence for this project does not license those files.

Python, xdelta3, and appimagetool are optional development/conversion/packaging
tools, rather than software bundled in the game packages. xdelta3 is installed
separately and is no longer shipped in the current source tree.
