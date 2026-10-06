#!/usr/bin/env bash
# Package the current build for release.
#
# usage: [CTR_ARCH=x86|arm64] ./package.sh <linux|windows> [version]
# CTR_ARCH only labels the package (default x86); it must match the build.
#
# Game assets are never packaged. The package is built from an explicit
# allowlist and the script fails if anything else ends up in it.

set -euo pipefail

PLATFORM="${1:-}"

# Default the version from VERSION, the same file CMake reads. The previous
# hardcoded beta-7_1 default was a second copy of the version and drifted from
# the build silently. CI passes an explicit version (branch + short SHA), so
# this only affects local packaging.
VERSION_FILE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/VERSION"
if [[ ! -s "${VERSION_FILE}" ]]; then
    echo "package.sh: ${VERSION_FILE} is missing or empty" >&2
    exit 1
fi
DEFAULT_VERSION="$(tr -d '[:space:]' < "${VERSION_FILE}")"
VERSION="${2:-${DEFAULT_VERSION}}"
BUILD_DIR="${BUILD_DIR:-build}"
DIST_DIR="${DIST_DIR:-dist}"
ARCH="${CTR_ARCH:-x86}"
case "${ARCH}" in
    x86|arm64) ;;
    *) echo "package.sh: CTR_ARCH must be x86 or arm64" >&2; exit 2 ;;
esac

case "${PLATFORM}" in
    linux)
        BINARY_NAME="ctr_native"
        RUN_HINT="./ctr_native"
        ;;
    windows)
        BINARY_NAME="Crash Team Racing - Turbocharged.exe"
        RUN_HINT="Crash Team Racing - Turbocharged.exe"
        ;;
    *)
        echo "usage: $0 <linux|windows> [version]" >&2
        exit 2
        ;;
esac

PACKAGE_NAME="ctr-turbocharged-${VERSION}-${PLATFORM}-${ARCH}"
PACKAGE_DIR="${DIST_DIR}/${PACKAGE_NAME}"
BINARY_PATH="${BUILD_DIR}/${BINARY_NAME}"

if [[ ! -f "${BINARY_PATH}" ]]; then
    echo "ERROR: missing executable: ${BINARY_PATH}" >&2
    echo "Build the game first." >&2
    exit 1
fi

rm -rf "${PACKAGE_DIR}"
mkdir -p "${PACKAGE_DIR}/assets"

cp "${BINARY_PATH}" "${PACKAGE_DIR}/"
cp LICENSE "${PACKAGE_DIR}/"
cp THIRD_PARTY_NOTICES.md "${PACKAGE_DIR}/"
mkdir -p "${PACKAGE_DIR}/licenses"
license_files=(
    MPL-2.0.txt SDL-zlib.txt HIDAPI-BSD.txt stb-MIT.txt
    yuv2rgb-BSD.txt SDL-EDID-MIT.txt SDL-fdlibm.txt
)
for license_file in "${license_files[@]}"; do
    cp "licenses/$license_file" "${PACKAGE_DIR}/licenses/"
done
# Project-owned Ghost Replay overlay image, not a game asset.
cp assets/dualshock.png "${PACKAGE_DIR}/assets/"
# Only the redistributable fonts, paired with full licences and attribution.
# Never copy this directory wholesale: it may contain player-supplied fonts.
mkdir -p "${PACKAGE_DIR}/assets/fonts"
font_files=(FuzzyBubbles-Bold.ttf FuzzyBubbles-OFL.txt LuckiestGuy-Regular.ttf
    LuckiestGuy-Apache-2.0.txt LuckiestGuy-NOTICE.txt README.md)
for font_file in "${font_files[@]}"; do
    cp "assets/fonts/$font_file" "${PACKAGE_DIR}/assets/fonts/"
done

if [[ "${PLATFORM}" == "linux" && "${ARCH}" == "arm64" ]]; then
    REQUIREMENTS="Linux ARM64 requirements (experimental 64-bit build):
- aarch64 Linux with a 64-bit glibc
- OpenGL 3.3 (or compatible) capable GPU/driver
- X11 or Wayland runtime libraries
- ALSA/PulseAudio/PipeWire runtime libraries

If the game does not launch, run it from a terminal and include:
- distro/version
- GPU/driver
- terminal output
- output of: ldd ./ctr_native"
elif [[ "${PLATFORM}" == "windows" && "${ARCH}" == "arm64" ]]; then
    REQUIREMENTS="Windows ARM64 requirements (experimental 64-bit build):
- Windows 11 on ARM
- OpenGL 3.3 capable GPU/driver"
elif [[ "${PLATFORM}" == "linux" ]]; then
    REQUIREMENTS="Linux requirements:
- x86_64 Linux capable of running 32-bit/i386 binaries
- 32-bit glibc runtime
- 32-bit OpenGL/Mesa or vendor OpenGL driver
- OpenGL 3.3 capable GPU/driver
- 32-bit X11 or Wayland runtime libraries
- 32-bit ALSA/PulseAudio/PipeWire runtime libraries

If the game does not launch, run it from a terminal and include:
- distro/version
- GPU/driver
- terminal output
- output of: ldd ./ctr_native"
else
    REQUIREMENTS="Windows requirements:
- 32-bit or 64-bit Windows
- OpenGL 3.3 capable GPU/driver"
fi

cat >"${PACKAGE_DIR}/README.txt" <<EOF
Crash Team Racing: Turbocharged ${PLATFORM} ${ARCH} ${VERSION} build

Game assets are not included. You must provide your own copy of the game.

Simple setup:
- Run the game and choose your own NTSC-U retail CTR raw BIN image in the
  disc setup popup. It validates the image, then copies it automatically to
  assets/ctr-u.bin. Your original file is kept. Existing images require
  confirmation before replacement.
- Alternatively, place the image manually at:

assets/
  ctr-u.bin

- Run:

${RUN_HINT}

First-time setup also offers the optional Crash-a-Like menu/HUD font.
Choose the extracted TTF to check and copy it to assets/fonts/crash-a-like.ttf.
If you do not already have it, choose Open README and follow First-time setup
> Add the optional Crash-a-Like font, or choose Use Luckiest Guy: it is bundled.
README: https://github.com/CameronRedmore/Crash-Team-Racing-Turbocharged#3-add-the-optional-crash-a-like-font
After import, select Options > Interface > Font > CRASH-A-LIKE to use it.
Options > Interface > Font also offers LUCKIEST GUY and ORIGINAL.
The Turbocharged preset uses Crash-a-Like if installed, otherwise Luckiest Guy.
Bundled font licences and attributions are included beside the TTF files.

The disc image must be the common single-track raw PSX BIN layout:
MODE2/2352 sectors, with the data track starting at byte 0.
A cooked 2048-byte ISO does not preserve the XA/STR sector data needed for
audio and video playback.

Extracted asset override:

Extracted files are optional and mostly useful for development, modding, and
debugging. If present, they override files from ctr-u.bin.

assets/
  BIGFILE.BIG
  SOUNDS/KART.HWL
  TEST.STR
  XA/
    ENG.XNF
    ENG/EXTRA/S00.XA ... S05.XA
    ENG/GAME/S00.XA ... S20.XA
    MUSIC/S00.XA ... S01.XA

XA files must preserve CD-XA sector data. Use 2336-byte Mode2/Form2 sector data
or 2352-byte raw sectors. 2048-byte cooked XA extractions are not suitable.

${REQUIREMENTS}
EOF

if [[ "${PLATFORM}" == "linux" ]]; then
    chmod +x "${PACKAGE_DIR}/${BINARY_NAME}"
fi

# Refuse to ship anything outside the allowlist, so game data can never leak
# into a package even if it is sitting in the build or source tree.
expected="$(printf '%s\n' \
    "./${BINARY_NAME}" \
    ./LICENSE \
    ./README.txt \
    ./THIRD_PARTY_NOTICES.md \
    ./assets/dualshock.png \
    "${font_files[@]/#/./assets/fonts/}" \
    "${license_files[@]/#/./licenses/}" | LC_ALL=C sort)"
actual="$(cd "${PACKAGE_DIR}" && find . -type f | LC_ALL=C sort)"
if [[ "${actual}" != "${expected}" ]]; then
    echo "ERROR: package contents do not match the allowlist:" >&2
    diff <(echo "${expected}") <(echo "${actual}") >&2 || true
    exit 1
fi

(
    cd "${DIST_DIR}"
    if [[ "${PLATFORM}" == "linux" ]]; then
        ARCHIVE="${PACKAGE_NAME}.tar.gz"
        tar -czf "${ARCHIVE}" "${PACKAGE_NAME}"
    else
        ARCHIVE="${PACKAGE_NAME}.zip"
        rm -f "${ARCHIVE}"
        zip -qr "${ARCHIVE}" "${PACKAGE_NAME}"
    fi
    sha256sum "${ARCHIVE}" >"${ARCHIVE}.sha256"
    echo "Wrote ${DIST_DIR}/${ARCHIVE}"
    echo "Wrote ${DIST_DIR}/${ARCHIVE}.sha256"
)
