#!/usr/bin/env bash
# Package the current x86 build for release.
#
# usage: ./package.sh <linux|windows> [version]
#
# Game assets are never packaged. The package is built from an explicit
# allowlist and the script fails if anything else ends up in it.

set -euo pipefail

PLATFORM="${1:-}"
VERSION="${2:-beta-7_1}"
BUILD_DIR="${BUILD_DIR:-build}"
DIST_DIR="${DIST_DIR:-dist}"

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

PACKAGE_NAME="ctr-turbocharged-${VERSION}-${PLATFORM}-x86"
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
# Project-owned Ghost Replay overlay image, not a game asset.
cp assets/dualshock.png "${PACKAGE_DIR}/assets/"

if [[ "${PLATFORM}" == "linux" ]]; then
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
Crash Team Racing: Turbocharged ${PLATFORM} x86 ${VERSION} build

Game assets are not included. You must provide your own copy of the game.

Simple setup:
- Put your own NTSC-U retail CTR disc image at:

assets/
  ctr-u.bin

- Run:

${RUN_HINT}

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
    ./assets/dualshock.png | LC_ALL=C sort)"
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
