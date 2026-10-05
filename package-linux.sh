#!/usr/bin/env bash
# Package the current Linux x86 build as a tarball and AppImage.

exec "$(dirname "$0")/package-appimage.sh" "$@"
