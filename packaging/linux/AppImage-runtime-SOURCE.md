# AppImage runtime source and relinking

This archive accompanies Turbocharged's Linux AppImage. It contains:

- `type2-runtime-20251108.tar.gz`: runtime sources, build scripts, Makefile,
  linker script, and `patches/libfuse/mount.c.diff`.
- `fuse-3.15.0.tar.xz`: matching libfuse source, including its LGPL 2.1 licence
  and the GPL licence for separate utilities.
- `squashfuse-0.5.2.tar.gz`: matching squashfuse library source.
- `licenses/`: full notices for the runtime and its linked libraries.

The runtime release is `20251108`, commit
`dd6cebedcbddde9c82f89b011e8e1d40b6e43868`. Its embedded version string is
`https://github.com/AppImage/type2-runtime/commit/dd6cebe`.
The shipped i686 runtime's SHA-256 is
`e72ea0b140a0a16e680713238a6f30aad278b62c4ca17919c554864124515498`.

## Build a replacement runtime

Extract the runtime archive and follow its `BUILD.md`. Its Docker build uses
Alpine 3.21 and the `ARCH=i686` setting. Before building, put the embedded
version string above in `src/runtime/version`.

`scripts/common/install-dependencies.sh` applies the supplied libfuse patch,
builds libfuse 3.15.0 statically with Meson/Ninja, and builds squashfuse 0.5.2.
You can use the included source archives in place of that script's downloads
and edit libfuse before compiling. The Makefile in `src/runtime/` links with
libfuse and squashfuse, plus Alpine's musl, zstd, zlib, and mimalloc libraries.
The supplied scripts show the compiler flags, library order, and post-link
processing needed to produce `runtime-i686`. Compiler and dependency versions
can affect the output bytes; a modified runtime need not have the original hash.

To use your replacement with an existing Turbocharged AppImage:

1. Run the AppImage with `--appimage-extract` to create `squashfs-root/`.
2. Download appimagetool from its official releases.
3. Run the following command:

   ```sh
   ARCH=i686 appimagetool --runtime-file /path/to/runtime-i686 squashfs-root/ Turbocharged-modified.AppImage
   ```

No signing key is required to build or run a modified Turbocharged AppImage.
Turbocharged does not prevent replacement of the LGPL library or its runtime.
The game disc image and saves stay outside the AppImage.

## Upstream sources

- Runtime: https://github.com/AppImage/type2-runtime/tree/20251108
- libfuse: https://github.com/libfuse/libfuse/releases/tag/fuse-3.15.0
- squashfuse: https://github.com/vasi/squashfuse/tree/0.5.2
- musl: https://git.musl-libc.org/cgit/musl/
- mimalloc: https://github.com/microsoft/mimalloc
- zstd: https://github.com/facebook/zstd
- zlib: https://github.com/madler/zlib
