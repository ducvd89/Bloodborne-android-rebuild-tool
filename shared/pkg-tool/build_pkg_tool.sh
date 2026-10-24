#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Builds ps4-pkg-tool (github.com/xXJSONDeruloXx/ps4-pkg-tools at 45baeda: shadPS4's PKG/PFS code, GPL-2.0-or-later), the command line program that
# unpacks PS4 fake PKGs for the Rebuild Tool. Built out of tree from the source that fetch-ps4-pkg-tools.sh fetches, with our patches applied
# (patches/: system zlib, byte progress, 64-bit offsets for multi-GB PKGs, refusal of unsafe file names) to a copy of it in out/pkg-tool-src.
#   host    -> out/host/pkg-tool/ps4-pkg-tool        (tests/recomp-pkgs)
#   linux   -> out/linux/pkg-tool/ps4-pkg-tool       (the AppImage: built against an Ubuntu 22.04 sysroot (glibc 2.35) with the LLVM tarball's clang,
#              static libstdc++; needs BB_SYSROOT, BB_LLVM_ROOT, BB_ZLIB_PREFIX, see fetch-deps.sh)
#   windows -> out/windows/pkg-tool/ps4-pkg-tool.exe (cross-compiled with llvm-mingw, static; needs BB_MINGW_ROOT and a zlib for mingw in
#              BB_MINGW_ZLIB_PREFIX, see package-windows.sh)
# Usage: shared/pkg-tool/build_pkg_tool.sh [host|linux|windows]   (default: host)
# Source: $BB_PKG_TOOL_SRC, else .local-deps/src/ps4-pkg-tools (fetched on first use).
set -euo pipefail
cd -- "$(dirname -- "$0")/../.."
root=$PWD
which=${1:-host}
jobs=${BB_JOBS:-22}
run() { nice -n 19 "$@"; }
src=${BB_PKG_TOOL_SRC:-$root/.local-deps/src/ps4-pkg-tools}
[[ -f $src/externals/cryptopp/cryptlib.h ]] || BB_PKG_TOOL_DEST=$src "$root/shared/pkg-tool/fetch-ps4-pkg-tools.sh"
[[ -f $src/CMakeLists.txt && -f $src/externals/cryptopp/cryptlib.h ]] \
    || { echo "No ps4-pkg-tools source (set BB_PKG_TOOL_SRC): $src" >&2; exit 1; }
# The patched copy: src/ copied, everything else (the big vendored externals) linked.
copy=$root/out/pkg-tool-src
rm -rf "$copy"
mkdir -p "$copy"
for entry in "$src"/*; do
    [[ ${entry##*/} == src ]] || ln -s "$entry" "$copy/${entry##*/}"
done
cp -a "$src/src" "$copy/src"
for patch in "$root"/shared/pkg-tool/patches/*.patch; do
    patch -d "$copy" -p1 -s < "$patch" || { echo "Patch failed: $patch" >&2; exit 1; }
done
src=$copy
# The tool prints "Progress: N / M files" with plain "\n": linked into a pipe, stdout would be
# block-buffered and the caller would see nothing until the end. This object (linked into the
# executable, run before main) makes stdout line-buffered.
shim() {   # <build dir> <compiler command...> -> prints the object path
    local dir=$1; shift
    mkdir -p "$dir"
    cat > "$dir/linebuf.c" <<'C'
#include <stdio.h>
__attribute__((constructor)) static void bb_line_buffered_stdout(void) { setvbuf(stdout, NULL, _IOLBF, 0); }
C
    "$@" -O2 -c "$dir/linebuf.c" -o "$dir/linebuf.o"
    echo "$dir/linebuf.o"
}

build_host() {
    local out=$root/out/host/pkg-tool build=$root/out/host/pkg-tool-build
    local object; object=$(shim "$build" cc)
    run cmake -S "$src" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_PKG_QT_GUI=OFF \
        -DCRYPTOPP_BUILD_TESTING=OFF "-DCMAKE_EXE_LINKER_FLAGS=$object" \
        >"$build/cmake.log" 2>&1 || { tail -30 "$build/cmake.log" >&2; exit 1; }
    run ninja -C "$build" -j "$jobs" ps4-pkg-tool >"$build/build.log" 2>&1 || { tail -40 "$build/build.log" >&2; exit 1; }
    mkdir -p "$out"
    cp "$build/ps4-pkg-tool" "$out/ps4-pkg-tool"
    echo "Built $out/ps4-pkg-tool (host)"
}

# Linux x86-64 for the AppImage: runs on glibc 2.35 and newer.
build_linux() {
    local sysroot=${BB_SYSROOT:-} llvm=${BB_LLVM_ROOT:-} zprefix=${BB_ZLIB_PREFIX:-}
    [[ -x $llvm/bin/clang && -d $sysroot/usr/include ]] || { echo "Set BB_SYSROOT and BB_LLVM_ROOT (fetch-deps.sh)" >&2; exit 1; }
    [[ -f $zprefix/lib/libz.a ]] || { echo "No static zlib: set BB_ZLIB_PREFIX" >&2; exit 1; }
    local out=$root/out/linux/pkg-tool build=$root/out/linux/pkg-tool-build
    local object; object=$(shim "$build" "$llvm/bin/clang" --sysroot="$sysroot")
    sed -i 's|</usr/include/zlib.h>|<zlib.h>|' "$src/src/core/file_format/pkg.cpp"
    run cmake -S "$src" -B "$build" -G Ninja -DCMAKE_TOOLCHAIN_FILE="$root/linux-jammy-toolchain.cmake" -DBB_SYSROOT="$sysroot" -DBB_LLVM_ROOT="$llvm" \
        -DBB_RECOMP_DEPS_PREFIX="$zprefix" -DCMAKE_BUILD_TYPE=Release -DBUILD_PKG_QT_GUI=OFF -DCRYPTOPP_BUILD_TESTING=OFF \
        -DZLIB_LIBRARY="$zprefix/lib/libz.a" -DZLIB_INCLUDE_DIR="$zprefix/include" "-DCMAKE_EXE_LINKER_FLAGS=-static-libstdc++ -static-libgcc $object" \
        >"$build/cmake.log" 2>&1 || { tail -30 "$build/cmake.log" >&2; exit 1; }
    run ninja -C "$build" -j "$jobs" ps4-pkg-tool >"$build/build.log" 2>&1 || { tail -40 "$build/build.log" >&2; exit 1; }
    mkdir -p "$out"
    cp "$build/ps4-pkg-tool" "$out/ps4-pkg-tool"
    "$llvm/bin/llvm-strip" --strip-all "$out/ps4-pkg-tool"
    echo "Built $out/ps4-pkg-tool (Linux x86-64, glibc 2.35)"
}

# Windows (x86-64), cross-compiled with llvm-mingw; everything static, so the .exe needs only the system's own DLLs.
build_windows() {
    local mingw=${BB_MINGW_ROOT:-} zprefix=${BB_MINGW_ZLIB_PREFIX:-}
    [[ -x $mingw/bin/x86_64-w64-mingw32-clang ]] || { echo "No llvm-mingw: set BB_MINGW_ROOT" >&2; exit 1; }
    [[ -f $zprefix/lib/libzlibstatic.a || -f $zprefix/lib/libz.a ]] || { echo "No zlib for mingw: set BB_MINGW_ZLIB_PREFIX" >&2; exit 1; }
    local zlib=$zprefix/lib/libzlibstatic.a
    [[ -f $zlib ]] || zlib=$zprefix/lib/libz.a
    local out=$root/out/windows/pkg-tool build=$root/out/windows/pkg-tool-build
    local object; object=$(shim "$build" "$mingw/bin/x86_64-w64-mingw32-clang")
    sed -i 's|</usr/include/zlib.h>|<zlib.h>|' "$src/src/core/file_format/pkg.cpp"
    # The manifest that makes UTF-8 the program's code page: PKG paths with non-ASCII characters (a user name with accents).
    (cd "$root/windows" && "$mingw/bin/x86_64-w64-mingw32-windres" utf8.rc -O coff -o "$build/utf8.o")
    run cmake -S "$src" -B "$build" -G Ninja -DCMAKE_TOOLCHAIN_FILE="$root/windows/toolchain-mingw-x86_64.cmake" -DBB_MINGW_ROOT="$mingw" \
        -DCMAKE_BUILD_TYPE=Release -DBUILD_PKG_QT_GUI=OFF -DCRYPTOPP_BUILD_TESTING=OFF \
        -DZLIB_LIBRARY="$zlib" -DZLIB_INCLUDE_DIR="$zprefix/include" "-DCMAKE_EXE_LINKER_FLAGS=-static $object $build/utf8.o -lbcrypt -ladvapi32 -lws2_32 -luser32 -lshell32 -lole32 -lntdll -lmincore" "-DCMAKE_CXX_FLAGS=-D_WIN32_WINNT=0x0A00 -DNTDDI_VERSION=0x0A000006" \
        >"$build/cmake.log" 2>&1 || { tail -30 "$build/cmake.log" >&2; exit 1; }
    run ninja -C "$build" -j "$jobs" ps4-pkg-tool >"$build/build.log" 2>&1 || { tail -40 "$build/build.log" >&2; exit 1; }
    mkdir -p "$out"
    cp "$build/ps4-pkg-tool.exe" "$out/ps4-pkg-tool.exe" 2>/dev/null || cp "$build/ps4-pkg-tool" "$out/ps4-pkg-tool.exe"
    "$mingw/bin/llvm-strip" --strip-all "$out/ps4-pkg-tool.exe"
    echo "Built $out/ps4-pkg-tool.exe (Windows x86-64)"
}

case $which in
    linux) build_linux ;;
    windows) build_windows ;;
    host) build_host ;;
    all) build_host ;;
    *) echo 'usage: build_pkg_tool.sh [host|linux|windows]' >&2; exit 2 ;;
esac
