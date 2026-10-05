#!/usr/bin/env bash
# Produce the Linux tarball and an i686 AppImage from the same asset allowlist.
# usage: ./package-appimage.sh [version]
# BUILD_DIR and DIST_DIR match package.sh. Downloads are pinned and cached.
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")"
version="${1:-$(tr -d '[:space:]' < VERSION)}"
if [[ $# -gt 1 || ! "$version" =~ ^[A-Za-z0-9][A-Za-z0-9._+-]*$ ]]; then
    echo "usage: $0 [version consisting of letters, digits, dots, underscores, pluses, or hyphens]" >&2
    exit 2
fi
if [[ "$(uname -m)" != x86_64 ]]; then
    echo "AppImage packaging currently requires an x86_64 Linux build host." >&2
    exit 1
fi
for command in curl sha256sum readelf; do
    command -v "$command" >/dev/null || { echo "Missing packaging tool: $command" >&2; exit 1; }
done
build_dir="${BUILD_DIR:-build}"
if ! LC_ALL=C readelf -h "$build_dir/ctr_native" | awk '/Machine:.*Intel 80386/ {found = 1} END {exit !found}'; then
    echo "AppImage packaging requires the Linux i686 game executable." >&2
    exit 1
fi

# Existing package.sh checks its exact contents before we stage the AppDir.
./package.sh linux "$version"
dist_dir=$(cd "${DIST_DIR:-dist}" && pwd -P)
package_name="ctr-turbocharged-${version}-linux-x86"
cache_dir="${APPIMAGE_TOOL_CACHE:-.cache/appimage-tools}"
mkdir -p "$cache_dir"
cache_dir=$(cd "$cache_dir" && pwd -P)

download() {
    local url="$1" file="$2" checksum="$3"
    if [[ -f "$file" ]] && printf '%s  %s\n' "$checksum" "$file" | sha256sum --check --status; then
        return
    fi
    curl --fail --location --retry 3 "$url" -o "$file.part"
    printf '%s  %s\n' "$checksum" "$file.part" | sha256sum --check --status
    mv "$file.part" "$file"
}

tool="$cache_dir/appimagetool-1.9.1-x86_64.AppImage"
runtime="$cache_dir/runtime-20251108-i686"
download 'https://github.com/AppImage/appimagetool/releases/download/1.9.1/appimagetool-x86_64.AppImage' \
    "$tool" ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0
download 'https://github.com/AppImage/type2-runtime/releases/download/20251108/runtime-i686' \
    "$runtime" e72ea0b140a0a16e680713238a6f30aad278b62c4ca17919c554864124515498
download 'https://codeload.github.com/AppImage/type2-runtime/tar.gz/refs/tags/20251108' \
    "$cache_dir/type2-runtime-20251108.tar.gz" 4c4f6df4647c9f01f871d7edd3716d8aeeffda9d22ffbebe3fccd95a6ab52c95
download 'https://github.com/libfuse/libfuse/releases/download/fuse-3.15.0/fuse-3.15.0.tar.xz' \
    "$cache_dir/fuse-3.15.0.tar.xz" 70589cfd5e1cff7ccd6ac91c86c01be340b227285c5e200baa284e401eea2ca0
download 'https://github.com/vasi/squashfuse/archive/0.5.2.tar.gz' \
    "$cache_dir/squashfuse-0.5.2.tar.gz" db0238c5981dabbd80ee09ae15387f390091668ca060a7bc38047912491443d3
chmod +x "$tool"

staging=$(mktemp -d "$dist_dir/.appimage-staging.XXXXXX")
trap 'rm -rf "$staging"' EXIT
app_dir="$staging/CTR-Turbocharged.AppDir"
share_dir="$app_dir/usr/share/ctr-turbocharged"
mkdir -p "$share_dir" "$app_dir/usr/bin"
cp -a "$dist_dir/$package_name/." "$share_dir/"
mv "$share_dir/ctr_native" "$app_dir/usr/bin/ctr_native"
cp packaging/linux/AppRun "$app_dir/"
cp packaging/linux/ctr-turbocharged.desktop "$app_dir/"
cp -a licenses/appimage "$share_dir/licenses/"
# Supply the statically linked LGPL library's source and the runtime using it.
source_dir="$staging/AppImage-runtime-source"
mkdir -p "$source_dir"
cp "$cache_dir/type2-runtime-20251108.tar.gz" "$cache_dir/fuse-3.15.0.tar.xz" \
    "$cache_dir/squashfuse-0.5.2.tar.gz" "$source_dir/"
cp packaging/linux/AppImage-runtime-SOURCE.md "$source_dir/README.md"
cp -a licenses/appimage "$source_dir/licenses"
source_archive="$dist_dir/$package_name.AppImage-runtime-source.tar.gz"
tar -czf "$source_archive" -C "$staging" AppImage-runtime-source
cp "$source_archive" "$share_dir/AppImage-runtime-source.tar.gz"
cp sce_sys/icon0.png "$app_dir/ctr-turbocharged.png"
ln -s ctr-turbocharged.png "$app_dir/.DirIcon"
chmod +x "$app_dir/AppRun"
cat >> "$share_dir/README.txt" <<'EOF'

AppImage setup:
The launcher creates ${XDG_DATA_HOME:-$HOME/.local/share}/ctr-turbocharged.
On first launch, select your raw NTSC-U BIN in the disc setup popup. It checks
and copies the image into that directory's assets/ctr-u.bin, keeping your
original. Manual placement is also supported. Put the optional Crash-a-Like
font in assets/fonts/crash-a-like.ttf. Settings, saves, and mods
also live in that writable directory. To use another folder, launch with:
CTR_TURBOCHARGED_DATA_DIR=/absolute/path/to/game-data ./Turbocharged.AppImage

The AppImage still needs the host's 32-bit glibc, OpenGL driver, windowing, and
audio libraries. It does not bundle system libraries or the retail disc.
If FUSE is unavailable, use --appimage-extract-and-run before game arguments.
EOF

# Run the tool without FUSE and use a pinned runtime rather than its live one.
output="$dist_dir/$package_name.AppImage"
ARCH=i686 "$tool" --appimage-extract-and-run --runtime-file "$runtime" \
    --mksquashfs-opt -processors --mksquashfs-opt "${APPIMAGE_JOBS:-2}" "$app_dir" "$output"
chmod +x "$output"
(
    cd "$dist_dir"
    sha256sum "$package_name.AppImage" > "$package_name.AppImage.sha256"
    sha256sum "$package_name.AppImage-runtime-source.tar.gz" > "$package_name.AppImage-runtime-source.tar.gz.sha256"
)
echo "Wrote $output"
echo "Wrote $output.sha256"
echo "Wrote $source_archive"
echo "Wrote $source_archive.sha256"
