#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Measurement build of bb-recomp (command line only, in-process LLD: BB_RECOMP_BUNDLED), to time the recompile on an Android phone.
#   arm64/build.sh aarch64   fully static AArch64 glibc executable (run under `adb shell`), out/arm64/bb-recomp
#   arm64/build.sh x86_64    the same sources as a static x86-64 executable, for the PC baseline, out/x86-bundled/bb-recomp
# Not a release: no window (no SDL/ImGui), no AppImage. Runs at nice 19 with at most BB_JOBS (default 12) jobs.
# Needs the x86-64 LLVM 23.1.3 tarball, its sysroot (jammy), zlib/zstd and the prepared FEX tree of a normal checkout (fetch-deps.sh, fetch-fex.sh):
# BB_X86_DEPS (default ../Bloodborne-android-rebuild-tool/.local-deps) and BB_FEX_TREE (default ../Bloodborne-android-rebuild-tool/out/recomp-fex)
# are only read. Everything for AArch64 (LLVM 23.1.3 Linux-ARM64 tarball, arm64 sysroot, zlib, zstd) goes into .local-deps here.
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
root=$PWD
mode=${1:-aarch64}
J=${BB_JOBS:-12}
earlyarg=()
LLVM_VER=${BB_LLVM_VERSION:-23.1.3}
x86deps=${BB_X86_DEPS:-$root/../Bloodborne-android-rebuild-tool/.local-deps}
fextree=${BB_FEX_TREE:-$root/../Bloodborne-android-rebuild-tool/out/recomp-fex}
deps=$root/.local-deps
run() { nice -n 19 "$@"; }
x86llvm=$x86deps/llvm/x/LLVM-$LLVM_VER-Linux-X64
[[ -x $x86llvm/bin/clang ]] || { echo "no x86-64 LLVM at $x86llvm (set BB_X86_DEPS)" >&2; exit 1; }

# FEX tree (a copy: the checkout it comes from stays untouched)
fex=$root/out/fex-src
if [[ ! -f $fex/FEXCore/CMakeLists.txt ]]; then
    [[ -f $fextree/FEXCore/CMakeLists.txt ]] || { echo "no prepared FEX tree at $fextree (set BB_FEX_TREE, or run fetch-fex.sh)" >&2; exit 1; }
    mkdir -p "$fex" && cp -a "$fextree/." "$fex/"
fi

if [[ $mode == x86_64 ]]; then
    out=$root/out/x86-bundled
    toolchain=(-DBB_RECOMP_TOOLCHAIN_FILE="$root/linux-jammy-toolchain.cmake" -DBB_SYSROOT="$x86deps/sysroot-jammy" -DBB_LLVM_ROOT="$x86llvm")
    llvm=$x86llvm; prefix=$x86deps/deps/prefix-jammy; march=x86-64-v3
elif [[ $mode == aarch64 ]]; then
    out=$root/out/arm64
    llvm=$deps/llvm-arm64/x/LLVM-$LLVM_VER-Linux-ARM64
    sysroot=$deps/sysroot-jammy-arm64
    prefix=$deps/deps/prefix-jammy-arm64
    mkdir -p "$deps/llvm-arm64/dl" "$deps/llvm-arm64/x" "$deps/deps/dl"
    if [[ ! -f $llvm/lib/cmake/llvm/LLVMConfig.cmake ]]; then
        f=$deps/llvm-arm64/dl/LLVM-$LLVM_VER-Linux-ARM64.tar.zst
        [[ -f $f ]] || curl -fL -C - -o "$f" "https://github.com/llvm/llvm-project/releases/download/llvmorg-$LLVM_VER/LLVM-$LLVM_VER-Linux-ARM64.tar.zst"
        (cd "$deps/llvm-arm64/x" && zstd -d --long=31 -c "$f" | run tar -x)   # 1 GiB window, as for the x86-64 archive
    fi
    [[ -f $sysroot/usr/lib/aarch64-linux-gnu/libc.a ]] || run python3 fetch-sysroot.py "$sysroot" --arch arm64
    toolchain=(-DBB_RECOMP_TOOLCHAIN_FILE="$root/arm64/toolchain-aarch64-jammy.cmake" -DBB_SYSROOT="$sysroot" -DBB_LLVM_ROOT="$x86llvm")
    tc=(-DCMAKE_TOOLCHAIN_FILE="$root/arm64/toolchain-aarch64-jammy.cmake" -DBB_SYSROOT="$sysroot" -DBB_LLVM_ROOT="$x86llvm")
    if [[ ! -f $prefix/lib/libz.a ]]; then
        tar -xf "$x86deps/deps/dl/zlib-1.3.1.tar.xz" -C "$deps/deps" && rm -rf "$deps/deps/b-z-arm64"
        run cmake -S "$deps/deps/zlib-1.3.1" -B "$deps/deps/b-z-arm64" -G Ninja "${tc[@]}" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix" >/dev/null
        run cmake --build "$deps/deps/b-z-arm64" -j "$J" --target zlib zlibstatic >/dev/null
        cmake --install "$deps/deps/b-z-arm64" >/dev/null
        rm -f "$prefix"/lib/libz.so*
    fi
    if [[ ! -f $prefix/lib/libzstd.a ]]; then
        tar -xf "$x86deps/deps/dl/zstd-1.5.7.tar.gz" -C "$deps/deps" && rm -rf "$deps/deps/b-zstd-arm64"
        run cmake -S "$deps/deps/zstd-1.5.7/build/cmake" -B "$deps/deps/b-zstd-arm64" -G Ninja "${tc[@]}" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix" \
            -DZSTD_BUILD_PROGRAMS=OFF -DZSTD_BUILD_TESTS=OFF -DZSTD_BUILD_SHARED=OFF -DZSTD_BUILD_STATIC=ON >/dev/null
        run cmake --build "$deps/deps/b-zstd-arm64" -j "$J" >/dev/null
        cmake --install "$deps/deps/b-zstd-arm64" >/dev/null
    fi
    march=armv8-a   # FEX adds +crc; generic ARMv8 so that the binary runs on any phone
    # libc.a and libm.a linked whole and first (see bbrecomp.cmake: GOTPAGE_LO15 of static glibc wraps beyond 32 KB of GOT without it)
    early=$deps/early-libs
    if [[ ! -f $early/libc-early.a ]]; then
        mkdir -p "$early"
        cp "$sysroot/usr/lib/aarch64-linux-gnu/libc.a" "$early/libc-early.a"
        "$x86llvm/bin/llvm-ar" d "$early/libc-early.a" dso_handle.o dl-reloc-static-pie.o   # crtbeginT.o and crt1.o define these
        cp "$sysroot/usr/lib/aarch64-linux-gnu/libm.a" "$early/libm-early.a"
    fi
    earlyarg=(-DBB_RECOMP_EARLY_LIBS_DIR="$early")
else
    echo "usage: $0 aarch64|x86_64" >&2; exit 2
fi

mkdir -p out
run cmake -S . -B "$out" -G Ninja -DFEX_SOURCE_DIR="$fex" -DBB_RECOMP_BUNDLED=ON -DBB_RECOMP_VERSION="${BB_RECOMP_VERSION:-1.1.0}" \
    "${toolchain[@]}" "${earlyarg[@]}" -DBB_RECOMP_LLVM_DIR="$llvm/lib/cmake/llvm" -DBB_RECOMP_DEPS_PREFIX="$prefix" -DBB_RECOMP_MARCH="$march" \
    -DBB_RECOMP_JOBS="$J" > "$out.cfg.log" 2>&1 || { tail -30 "$out.cfg.log"; exit 1; }
run cmake --build "$out" -j "$J"
ls -l "$out/bb-recomp"
