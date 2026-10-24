#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Downloads the Android NDK r29 (29.0.14206865, Linux x86-64, 780 MB) into .local-deps/android/ndk/android-ndk-r29 (ignored by git).
#   ./fetch-ndk.sh
# Only the packages need it (package.sh, package-windows.sh): the AArch64 build of the x87 helpers, the libm link stub and the compiler-rt
# builtins archive that the recompiled game library links come from it. A developer build of bb-recomp for --so also uses it (--ndk or BB_NDK).
# The archive is checked against the SHA-1 Google publishes for it. BB_NDK_ZIP may name an archive you already have.
set -euo pipefail
cd -- "$(dirname -- "$0")"
dest=$PWD/.local-deps/android/ndk
sha1=87e2bb7e9be5d6a1c6cdf5ec40dd4e0c6d07c30b
[[ -x $dest/android-ndk-r29/toolchains/llvm/prebuilt/linux-x86_64/bin/clang ]] && { echo "NDK r29 already in $dest/android-ndk-r29"; exit 0; }
mkdir -p "$dest"
zip=${BB_NDK_ZIP:-$dest/android-ndk-r29-linux.zip}
[[ -f $zip ]] || curl -fL -C - -o "$zip" https://dl.google.com/android/repository/android-ndk-r29-linux.zip
echo "$sha1  $zip" | sha1sum -c - >&2
rm -rf "$dest/android-ndk-r29"
nice -n 19 unzip -q "$zip" -d "$dest"
echo "NDK r29 in $dest/android-ndk-r29"
