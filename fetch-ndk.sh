#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Downloads the Android NDK r29 (29.0.14206865) into .local-deps/android/ndk/android-ndk-r29 (ignored by git): the Linux x86-64 build
# (780 MB) on Linux, the macOS build (1 GB, universal binaries) on a Mac.
#   ./fetch-ndk.sh
# Only the packages need it (package.sh, package-windows.sh, package-macos.sh): the AArch64 build of the x87 helpers, the libm link stub and
# the compiler-rt builtins archive that the recompiled game library links come from it. A developer build of bb-recomp for --so also uses it
# (--ndk or BB_NDK).
# The archive is checked against the SHA-1 Google publishes for it. BB_NDK_ZIP may name an archive you already have.
set -euo pipefail
cd -- "$(dirname -- "$0")"
dest=$PWD/.local-deps/android/ndk
if [[ $(uname -s) == Darwin ]]; then
    host=darwin; sha1=03d29fbb57e3c05a7d53597dd011d856c1456a4f
    sha1check() { shasum -a 1 -c -; }
else
    host=linux; sha1=87e2bb7e9be5d6a1c6cdf5ec40dd4e0c6d07c30b
    sha1check() { sha1sum -c -; }
fi
[[ -x $dest/android-ndk-r29/toolchains/llvm/prebuilt/$host-x86_64/bin/clang ]] && { echo "NDK r29 already in $dest/android-ndk-r29"; exit 0; }
mkdir -p "$dest"
zip=${BB_NDK_ZIP:-$dest/android-ndk-r29-$host.zip}
[[ -f $zip ]] || curl -fL --progress-bar -C - -o "$zip" "https://dl.google.com/android/repository/android-ndk-r29-$host.zip"
echo "$sha1  $zip" | sha1check >&2
rm -rf "$dest/android-ndk-r29"
nice -n 19 unzip -q "$zip" -d "$dest"
echo "NDK r29 in $dest/android-ndk-r29"
