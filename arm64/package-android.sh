#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Makes out/android/: what the Android app ships in its APK to recompile the game on the phone (docs/NATIVE_CPU.md, "On the phone"), and the tarball of it.
#   arm64/package-android.sh            builds the static AArch64 bb-recomp (arm64/build.sh aarch64) first, then packs
#   BB_SKIP_BUILD=1 arm64/package-android.sh   packs the binary of an earlier build (out/arm64/bb-recomp)
# out/android/libbbrecomp.so (the stripped static executable: the APK ships it in jniLibs/arm64-v8a and runs it from nativeLibraryDir), bundle/ (the four link
# inputs of the .so, bundle.cpp), variants/*.txt (the variant catalog), licenses/, VERSION, README-ANDROID.md. Output: out/dist/bb-recomp-android-aarch64.tar.xz (+ .sha256).
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
root=$PWD
VERSION=${BB_RECOMP_VERSION:-1.2.0}
export BB_RECOMP_VERSION=$VERSION
run() { nice -n 19 "$@"; }
[[ ${BB_SKIP_BUILD:-0} == 1 ]] || arm64/build.sh aarch64
exe=out/arm64/bb-recomp
[[ -x $exe ]] || { echo "no $exe (run arm64/build.sh aarch64)" >&2; exit 1; }
LLVM_VER=${BB_LLVM_VERSION:-23.1.3}
x86llvm=${BB_X86_DEPS:-$root/.local-deps}/llvm/x/LLVM-$LLVM_VER-Linux-X64
ndk=${BB_NDK:-$root/.local-deps/android/ndk/android-ndk-r29}
[[ -d $ndk ]] || ./fetch-ndk.sh
nbin=$ndk/toolchains/llvm/prebuilt/linux-x86_64
builtins=$(ls "$nbin"/lib/clang/*/lib/linux/libclang_rt.builtins-aarch64-android.a | head -n 1)

out=out/android
rm -rf "$out"; mkdir -p "$out/bundle" "$out/variants" "$out/licenses"
"$x86llvm/bin/llvm-strip" -o "$out/libbbrecomp.so" "$exe"
chmod 755 "$out/libbbrecomp.so"
# the files the .so link needs, from the NDK (the same commands package.sh runs for the desktop packages)
run "$nbin/bin/clang" --target=aarch64-linux-android30 -O2 -fPIC -fvisibility=hidden -fno-stack-protector -ffp-contract=off -c helpers/bbr_x87_all.c -o "$out/bundle/helpers-aarch64.o"
run "$nbin/bin/clang" --target=aarch64-linux-android30 -shared -nostdlib -Wl,-soname,libbbcpu.so -x c /dev/null -o "$out/bundle/libbbcpu-stub.so"
cp "$nbin/sysroot/usr/lib/aarch64-linux-android/30/libm.so" "$out/bundle/libm.so"
cp "$builtins" "$out/bundle/"
cp variants/*.txt "$out/variants/"
# licenses of what is linked into the binary (as the desktop packages carry them)
fex=$root/out/fex-src
[[ -f $fex/LICENSE ]] || fex=$root/out/recomp-fex
cp "$fex/LICENSE" "$out/licenses/FEX-MIT.txt"
cp .local-deps/llvm/LICENSE.TXT "$out/licenses/LLVM-Apache-2.0-with-LLVM-exception.txt"
cp helpers/NOTICE "$out/licenses/x87-helpers-NOTICE.txt"
cp LICENSE "$out/licenses/GPL-3.0.txt"; cp NOTICES "$out/licenses/NOTICES.txt"; cp ADDITIONAL_TERMS.md "$out/licenses/ADDITIONAL_TERMS.md"
hash=$(git rev-parse --short=12 HEAD 2>/dev/null || echo unknown)
dirty=; [[ -z $(git status --porcelain --untracked-files=no 2>/dev/null) ]] || dirty=-dirty
cat > "$out/VERSION" <<VER
bb-recomp $VERSION
git $hash$dirty
abi_version 2
llvm $LLVM_VER
target aarch64-linux (static, glibc), --target aarch64 only
VER
cp arm64/README-ANDROID.md "$out/README-ANDROID.md"
# a self-check: the binary must report its version
[[ $(file -b "$out/libbbrecomp.so") == *"ARM aarch64"* ]] || { echo "libbbrecomp.so is not an AArch64 executable" >&2; exit 1; }
mkdir -p out/dist
tar -C out -cf - android | xz -9e -T0 > out/dist/bb-recomp-android-aarch64.tar.xz
(cd out/dist && sha256sum bb-recomp-android-aarch64.tar.xz > bb-recomp-android-aarch64.tar.xz.sha256)
du -ab "$out" | sort -k2 | awk '{printf "%10d  %s\n", $1, $2}'
ls -l out/dist/bb-recomp-android-aarch64.tar.xz
