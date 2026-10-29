#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Makes out/android/: what the Android app ships in its APK to recompile the game on the phone (docs/NATIVE_CPU.md, "On the phone"), and the tarball of it.
#   arm64/package-android.sh            builds the Android NDK bb-recomp (arm64/build.sh android: bionic, dynamically linked PIE) first, then packs
#   BB_SKIP_BUILD=1 arm64/package-android.sh   packs the binary of an earlier build (out/android-ndk/bb-recomp)
#   BB_ANDROID_BINARY=glibc arm64/package-android.sh   packs the static glibc build instead (arm64/build.sh aarch64, out/arm64/bb-recomp): it is killed by
#                                       the app seccomp filter of Android 16 and newer, kept for the measurement runs (arm64/run-on-device.sh) only
# out/android/libbbrecomp.so (the stripped executable: the APK ships it in jniLibs/arm64-v8a and runs it from nativeLibraryDir), bundle/ (the four link
# inputs of the .so, bundle.cpp), variants/*.txt (the variant catalog), licenses/, VERSION, README-ANDROID.md. Output: out/dist/bb-recomp-android-aarch64.tar.xz (+ .sha256).
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
root=$PWD
VERSION=${BB_RECOMP_VERSION:-1.3.0}
export BB_RECOMP_VERSION=$VERSION
run() { nice -n 19 "$@"; }
binary=${BB_ANDROID_BINARY:-ndk}
case $binary in
    ndk) bmode=android; exe=out/android-ndk/bb-recomp
         target='aarch64-linux-android30 (NDK r29, bionic, dynamic PIE), --target aarch64 only' ;;
    glibc) bmode=aarch64; exe=out/arm64/bb-recomp
           target='aarch64-linux (static, glibc), --target aarch64 only' ;;
    *) echo "BB_ANDROID_BINARY is ndk or glibc" >&2; exit 2 ;;
esac
[[ ${BB_SKIP_BUILD:-0} == 1 ]] || arm64/build.sh "$bmode"
[[ -x $exe ]] || { echo "no $exe (run arm64/build.sh $bmode)" >&2; exit 1; }
LLVM_VER=${BB_LLVM_VERSION:-23.1.3}
x86deps=${BB_X86_DEPS:-$root/../Bloodborne-android-rebuild-tool/.local-deps}   # as in build.sh
x86llvm=$x86deps/llvm/x/LLVM-$LLVM_VER-Linux-X64
ndk=${BB_NDK:-$x86deps/android/ndk/android-ndk-r29}
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
# CATALOG_HASH: the catalog hash (docs/COMPILER.md, "Catalog hash"; the same as bb-recomp --print-catalog-hash --variants <dir>), so the app can tell a library built from an older catalog
python3 - "$out/variants" > "$out/variants/CATALOG_HASH" <<'PY'
import sys, os
d = sys.argv[1]; h = 0xcbf29ce484222325
for n in sorted(f for f in os.listdir(d) if f.endswith(".txt")):
    for b in (n.encode(), b"\0", open(os.path.join(d, n), "rb").read(), b"\0"):
        for c in b: h = ((h ^ c) * 0x100000001b3) & 0xffffffffffffffff
print("%016x" % h)
PY
# licenses of what is linked into the binary (as the desktop packages carry them)
fex=$root/out/fex-src
[[ -f $fex/LICENSE ]] || fex=$root/out/recomp-fex
cp "$fex/LICENSE" "$out/licenses/FEX-MIT.txt"
cp "$x86deps/llvm/LICENSE.TXT" "$out/licenses/LLVM-Apache-2.0-with-LLVM-exception.txt"
cp helpers/NOTICE "$out/licenses/x87-helpers-NOTICE.txt"
cp LICENSE "$out/licenses/GPL-3.0.txt"; cp NOTICES "$out/licenses/NOTICES.txt"; cp ADDITIONAL_TERMS.md "$out/licenses/ADDITIONAL_TERMS.md"
hash=$(git rev-parse --short=12 HEAD 2>/dev/null || echo unknown)
dirty=; [[ -z $(git status --porcelain --untracked-files=no 2>/dev/null) ]] || dirty=-dirty
cat > "$out/VERSION" <<VER
bb-recomp $VERSION
git $hash$dirty
abi_version 2
llvm $LLVM_VER
target $target
VER
cp arm64/README-ANDROID.md "$out/README-ANDROID.md"
# a self-check: the binary must report its version
[[ $(file -b "$out/libbbrecomp.so") == *"ARM aarch64"* ]] || { echo "libbbrecomp.so is not an AArch64 executable" >&2; exit 1; }
if [[ $binary == ndk ]]; then
    # ... and the NDK build must load on a phone: bionic's linker as interpreter, nothing but libc, libm and libdl needed (no libz, no libstdc++, no libpthread)
    needed=$("$x86llvm/bin/llvm-readelf" -d "$out/libbbrecomp.so" | sed -n 's/.*Shared library: \[\(.*\)\]/\1/p' | sort | tr '\n' ' ')
    [[ $needed == "libc.so libdl.so libm.so " ]] || { echo "libbbrecomp.so needs unexpected libraries: $needed" >&2; exit 1; }
    "$x86llvm/bin/llvm-readelf" -l "$out/libbbrecomp.so" | grep -q 'program interpreter: /system/bin/linker64' || { echo "libbbrecomp.so is not a bionic executable" >&2; exit 1; }
fi
mkdir -p out/dist
tar -C out -cf - android | xz -9e -T0 > out/dist/bb-recomp-android-aarch64.tar.xz
(cd out/dist && sha256sum bb-recomp-android-aarch64.tar.xz > bb-recomp-android-aarch64.tar.xz.sha256)
du -ab "$out" | sort -k2 | awk '{printf "%10d  %s\n", $1, $2}'
ls -l out/dist/bb-recomp-android-aarch64.tar.xz
