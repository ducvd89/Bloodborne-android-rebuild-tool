#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Downloads and builds what the macOS build of the Rebuild Tool needs, into .local-deps (ignored by git). Needs network the first time.
#   ./fetch-deps-macos.sh
# The macOS counterpart of fetch-deps.sh, for Apple Silicon (arm64) Macs:
# - LLVM 23.1.3: the official release tarball (macOS arm64: static libraries for every target, headers, clang, lld). clang is the compiler;
#   the X86 + AArch64 code generators and LLD's ELF linker are linked into bb-recomp.
# - zstd 1.5.7 (static, LLVMSupport needs it) and SDL3 3.4.18 (static, Cocoa) built with that clang; Dear ImGui (pinned commit).
#   zlib is the system's (/usr/lib/libz.1.dylib is part of every macOS).
# - the Android NDK r29 for macOS (fetch-ndk.sh).
set -euo pipefail
cd -- "$(dirname -- "$0")"
LLVM_VER=${BB_LLVM_VERSION:-23.1.3}
SDL_VER=${BB_SDL_VERSION:-3.4.18}
IMGUI_COMMIT=${BB_IMGUI_COMMIT:-f1cc2ae15e53a861a874c3034aae6798fde194ab}
deps=${BB_RECOMP_DEPS:-$PWD/.local-deps}
llvm=$deps/llvm/x/LLVM-$LLVM_VER-macOS-ARM64
prefix=$deps/deps/prefix-macos
J=${BB_JOBS:-$(sysctl -n hw.ncpu)}
export MACOSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET:-14.4}
[[ $(uname -s) == Darwin && $(uname -m) == arm64 ]] || { echo "fetch-deps-macos.sh: needs an Apple Silicon Mac" >&2; exit 1; }
mkdir -p "$deps/llvm/dl" "$deps/llvm/x" "$deps/deps/dl" "$deps/sdl/dl" "$deps/sdl/src" "$deps/imgui/dl"
cc=(-DCMAKE_C_COMPILER="$llvm/bin/clang" -DCMAKE_CXX_COMPILER="$llvm/bin/clang++" -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET="$MACOSX_DEPLOYMENT_TARGET")

if [[ ! -x $llvm/bin/clang ]]; then
    f=$deps/llvm/dl/LLVM-$LLVM_VER-macOS-ARM64.tar.xz
    [[ -f $f ]] || curl -fL -C - -o "$f" "https://github.com/llvm/llvm-project/releases/download/llvmorg-$LLVM_VER/LLVM-$LLVM_VER-macOS-ARM64.tar.xz"
    (cd "$deps/llvm/x" && nice -n 19 tar -xf "$f")
fi
[[ -f $deps/llvm/LICENSE.TXT ]] || curl -fL -o "$deps/llvm/LICENSE.TXT" "https://raw.githubusercontent.com/llvm/llvm-project/llvmorg-$LLVM_VER/llvm/LICENSE.TXT"

if [[ ! -f $prefix/lib/libzstd.a ]]; then
    [[ -f $deps/deps/dl/zstd-1.5.7.tar.gz ]] || curl -fL -o "$deps/deps/dl/zstd-1.5.7.tar.gz" https://github.com/facebook/zstd/releases/download/v1.5.7/zstd-1.5.7.tar.gz
    mkdir -p "$deps/deps/src2" && tar -xf "$deps/deps/dl/zstd-1.5.7.tar.gz" -C "$deps/deps/src2"
    rm -rf "$deps/deps/b2m"
    nice -n 19 cmake -S "$deps/deps/src2/zstd-1.5.7/build/cmake" -B "$deps/deps/b2m" -G Ninja "${cc[@]}" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix" \
        -DZSTD_BUILD_PROGRAMS=OFF -DZSTD_BUILD_TESTS=OFF -DZSTD_BUILD_SHARED=OFF -DZSTD_BUILD_STATIC=ON >/dev/null
    nice -n 19 cmake --build "$deps/deps/b2m" -j "$J" >/dev/null && cmake --install "$deps/deps/b2m" >/dev/null
fi

# SDL3, static, video only (Cocoa + Metal renderer): the window needs nothing else.
sdlsrc=$deps/sdl/src/SDL3-$SDL_VER
if [[ ! -f $deps/sdl/prefix-macos/lib/libSDL3.a ]]; then
    [[ -f $deps/sdl/dl/SDL3-$SDL_VER.tar.gz ]] || curl -fL -o "$deps/sdl/dl/SDL3-$SDL_VER.tar.gz" "https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VER/SDL3-$SDL_VER.tar.gz"
    [[ -d $sdlsrc ]] || tar -xf "$deps/sdl/dl/SDL3-$SDL_VER.tar.gz" -C "$deps/sdl/src"
    rm -rf "$deps/sdl/build-macos"
    nice -n 19 cmake -S "$sdlsrc" -B "$deps/sdl/build-macos" -G Ninja "${cc[@]}" -DCMAKE_BUILD_TYPE=Release -DSDL_STATIC=ON -DSDL_SHARED=OFF \
        -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF -DSDL_AUDIO=OFF -DSDL_CAMERA=OFF -DSDL_JOYSTICK=OFF -DSDL_HIDAPI=OFF -DSDL_HAPTIC=OFF \
        -DSDL_SENSOR=OFF -DSDL_POWER=OFF -DSDL_GPU=OFF -DCMAKE_INSTALL_PREFIX="$deps/sdl/prefix-macos" >"$deps/sdl/build-macos.log" 2>&1 \
        || { tail -20 "$deps/sdl/build-macos.log" >&2; exit 1; }
    nice -n 19 cmake --build "$deps/sdl/build-macos" -j "$J" >>"$deps/sdl/build-macos.log" 2>&1 || { tail -20 "$deps/sdl/build-macos.log" >&2; exit 1; }
    cmake --install "$deps/sdl/build-macos" >/dev/null
fi
if ! ls "$deps"/imgui/imgui-*/imgui.cpp >/dev/null 2>&1; then
    [[ -f $deps/imgui/dl/imgui.tar.gz ]] || curl -fL -o "$deps/imgui/dl/imgui.tar.gz" "https://github.com/ocornut/imgui/archive/$IMGUI_COMMIT.tar.gz"
    tar -xf "$deps/imgui/dl/imgui.tar.gz" -C "$deps/imgui"
fi
./fetch-ndk.sh
echo "LLVM: $llvm"; echo "deps prefix: $prefix"; echo "SDL3: $deps/sdl/prefix-macos"; ls -d "$deps"/imgui/imgui-*
