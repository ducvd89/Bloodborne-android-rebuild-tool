#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Command line build of bb-recomp (no window; in-process LLD: BB_RECOMP_BUNDLED) that runs on an Android phone (arm64/package-android.sh makes out/android/ from it).
#   arm64/build.sh android   Android NDK r29 executable (bionic, PIE, libc++ static; needs only libc.so, libm.so, libdl.so), out/android-ndk/bb-recomp: what the app ships
#   arm64/build.sh aarch64   fully static AArch64 glibc executable (run under `adb shell`), out/arm64/bb-recomp: the measurement build; the app's seccomp filter (Android 16+)
#                            kills it, because glibc makes syscalls bionic never does (rseq, set_robust_list, faccessat2 ...)
#   arm64/build.sh x86_64    the same sources as a static x86-64 executable, for the PC baseline, out/x86-bundled/bb-recomp
# The android and aarch64 builds have no X86 code generator (BB_RECOMP_NO_X86: only --target aarch64 works); the x86_64 one keeps both. No window (no SDL/ImGui), no AppImage. Runs at nice 19 with at most BB_JOBS (default 12) jobs.
# Needs the x86-64 LLVM 23.1.3 tarball (its llvm-tblgen builds the cross LLVM), zlib/zstd and the prepared FEX tree of a normal checkout (fetch-deps.sh, fetch-fex.sh):
# BB_X86_DEPS (default ../Bloodborne-android-rebuild-tool/.local-deps) and BB_FEX_TREE (default ../Bloodborne-android-rebuild-tool/out/recomp-fex) are only read,
# except that the android mode builds its own LLVM + LLD 23.1.3 (llvm/build-android), zlib and zstd (deps/prefix-android) there, with the LLVM sources and the NDK r29
# (BB_NDK) it finds there: the first run takes about 4 minutes on 22 cores (LLVM + LLD is most of it), later runs only the bb-recomp build. The android build is made with
# -ffp-contract=off (the x86-64 builds have no FMA, so the floating point inside LLVM must not fuse either, or the .so would differ) and API level BB_ANDROID_API (30).
# Everything for AArch64 glibc (LLVM 23.1.3 Linux-ARM64 tarball, arm64 sysroot, zlib, zstd) goes into .local-deps here.
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
    earlyarg=(-DBB_RECOMP_EARLY_LIBS_DIR="$early" -DBB_RECOMP_NO_X86=ON)   # no X86 code generator: the phone only needs --target aarch64
elif [[ $mode == android ]]; then
    # NDK r29 (bionic): clang and lld of the NDK build a dynamically linked PIE executable (libc.so, libm.so, libdl.so; libc++ static) that the app can run:
    # nothing glibc does behind the app's seccomp filter (rseq, set_robust_list, faccessat2 ...) is in it. LLVM + LLD, zlib and zstd are built for it first.
    out=$root/out/android-ndk
    ndk=${BB_NDK:-$x86deps/android/ndk/android-ndk-r29}
    [[ -f $ndk/build/cmake/android.toolchain.cmake ]] || { echo "no Android NDK r29 at $ndk (set BB_NDK, or run fetch-ndk.sh)" >&2; exit 1; }
    api=${BB_ANDROID_API:-30}
    prefix=$x86deps/deps/prefix-android
    lb=$x86deps/llvm/build-android
    src=$x86deps/llvm/src/llvm-project-$LLVM_VER.src
    [[ -d $src/llvm ]] || { echo "no LLVM $LLVM_VER sources at $src (package-windows.sh or fetch-deps.sh fetch them)" >&2; exit 1; }
    tcfile=$ndk/build/cmake/android.toolchain.cmake
    ndkargs=(-DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-$api -DANDROID_STL=c++_static)
    # -ffp-contract=off: the same floating point results as the x86-64 builds (see CMakeLists.txt); -g0: the NDK's default -g makes the libraries 10x larger
    tc=(-DCMAKE_TOOLCHAIN_FILE="$tcfile" "${ndkargs[@]}" -DCMAKE_FIND_ROOT_PATH="$prefix" "-DCMAKE_C_FLAGS=-ffp-contract=off -g0" "-DCMAKE_CXX_FLAGS=-ffp-contract=off -g0")
    toolchain=(-DBB_RECOMP_TOOLCHAIN_FILE="$tcfile" -DBB_ANDROID_PLATFORM=android-$api)
    mkdir -p "$prefix" "$x86deps/deps"
    # zlib and zstd, static (LLVM's CMake and bbrecomp.cmake find them again by CMAKE_FIND_ROOT_PATH / the deps prefix)
    if [[ ! -f $prefix/lib/libz.a ]]; then
        tar -xf "$x86deps/deps/dl/zlib-1.3.1.tar.xz" -C "$x86deps/deps" && rm -rf "$x86deps/deps/b-z-android"
        run cmake -S "$x86deps/deps/zlib-1.3.1" -B "$x86deps/deps/b-z-android" -G Ninja "${tc[@]}" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix" >/dev/null
        run cmake --build "$x86deps/deps/b-z-android" -j "$J" >/dev/null
        cmake --install "$x86deps/deps/b-z-android" >/dev/null
        rm -f "$prefix"/lib/libz.so*
    fi
    if [[ ! -f $prefix/lib/libzstd.a ]]; then
        tar -xf "$x86deps/deps/dl/zstd-1.5.7.tar.gz" -C "$x86deps/deps" && rm -rf "$x86deps/deps/b-zstd-android"
        run cmake -S "$x86deps/deps/zstd-1.5.7/build/cmake" -B "$x86deps/deps/b-zstd-android" -G Ninja "${tc[@]}" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix" \
            -DZSTD_BUILD_PROGRAMS=OFF -DZSTD_BUILD_TESTS=OFF -DZSTD_BUILD_SHARED=OFF -DZSTD_BUILD_STATIC=ON >/dev/null
        run cmake --build "$x86deps/deps/b-zstd-android" -j "$J" >/dev/null
        cmake --install "$x86deps/deps/b-zstd-android" >/dev/null
    fi
    # LLVM + LLD for the phone: the AArch64 target only (BB_RECOMP_NO_X86), no tools, the libraries bb-recomp links (the list of package-windows.sh without X86).
    # The release build of the official tarballs (no assertions), so the code it generates is the same as the PC tool's.
    if [[ ! -f $lb/lib/liblldELF.a ]]; then
        run cmake -S "$src/llvm" -B "$lb" -G Ninja "${tc[@]}" -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_PROJECTS=lld -DLLVM_TARGETS_TO_BUILD=AArch64 \
            -DLLVM_NATIVE_TOOL_DIR="$x86llvm/bin" -DLLVM_HOST_TRIPLE=aarch64-unknown-linux-android$api -DLLVM_ENABLE_ZLIB=FORCE_ON -DLLVM_ENABLE_ZSTD=FORCE_ON -DLLVM_USE_STATIC_ZSTD=ON \
            -DZLIB_ROOT="$prefix" -DZLIB_USE_STATIC_LIBS=ON -Dzstd_ROOT="$prefix" \
            -DLLVM_ENABLE_LIBXML2=OFF -DLLVM_ENABLE_TERMINFO=OFF -DLLVM_INCLUDE_TESTS=OFF -DLLVM_INCLUDE_EXAMPLES=OFF -DLLVM_INCLUDE_BENCHMARKS=OFF -DLLVM_BUILD_TOOLS=OFF \
            -DLLVM_INCLUDE_UTILS=OFF -DLLVM_INCLUDE_DOCS=OFF -DLLD_BUILD_TOOLS=OFF -DLLVM_ENABLE_ASSERTIONS=OFF -DLLVM_ENABLE_PLUGINS=OFF -DLLVM_ENABLE_BINDINGS=OFF \
            -DLLVM_ENABLE_Z3_SOLVER=OFF > "$root/out/llvm-android-cfg.log" 2>&1 || { tail -30 "$root/out/llvm-android-cfg.log"; exit 1; }
        run cmake --build "$lb" -j "$J" --target lldELF lldCommon LLVMAArch64CodeGen LLVMAArch64AsmParser LLVMOrcJIT LLVMPasses LLVMIRReader LLVMLTO LLVMDTLTO LLVMObject \
            LLVMTargetParser LLVMOption LLVMBinaryFormat LLVMProfileData LLVMDebugInfoDWARF LLVMExecutionEngine LLVMAArch64Desc LLVMAArch64Info LLVMAArch64Utils LLVMMCJIT \
            LLVMRuntimeDyld LLVMJITLink LLVMOrcTargetProcess LLVMOrcShared > "$root/out/llvm-android-build.log" 2>&1 || { tail -40 "$root/out/llvm-android-build.log"; exit 1; }
    fi
    llvm=$lb
    march=armv8-a   # FEX adds +crc; generic ARMv8 so that the binary runs on any phone
    earlyarg=(-DBB_RECOMP_NO_X86=ON "-DBB_RECOMP_LLD_INCLUDE=$src/lld/include;$lb/tools/lld/include")
else
    echo "usage: $0 aarch64|x86_64|android" >&2; exit 2
fi

mkdir -p out
run cmake -S . -B "$out" -G Ninja -DFEX_SOURCE_DIR="$fex" -DBB_RECOMP_BUNDLED=ON -DBB_RECOMP_VERSION="${BB_RECOMP_VERSION:-1.2.0}" \
    "${toolchain[@]}" "${earlyarg[@]}" -DBB_RECOMP_LLVM_DIR="$llvm/lib/cmake/llvm" -DBB_RECOMP_DEPS_PREFIX="$prefix" -DBB_RECOMP_MARCH="$march" \
    -DBB_RECOMP_JOBS="$J" > "$out.cfg.log" 2>&1 || { tail -30 "$out.cfg.log"; exit 1; }
run cmake --build "$out" -j "$J"
ls -l "$out/bb-recomp"
