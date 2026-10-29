#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Builds the macOS release of the Bloodborne Android Rebuild Tool, for Apple Silicon (M1 and newer, macOS 14.4+):
#   out/dist/Bloodborne Android Rebuild Tool.app   and   out/dist/bb-recomp-macos-arm64.zip (the .app, zipped with ditto)
#   ./package-macos.sh [--test <folder with the game's PKGs> [--compare <libbbaot.so of another build>]]
#
# The .app holds the program (window and command line in one binary: Dear ImGui + SDL3, LLVM 23 with the X86 and AArch64 code generators and
# LLD linked in statically), ps4-pkg-tool, the AArch64 x87 helper object and the link stubs, as the AppImage does. It needs nothing from the
# system but macOS itself: no Homebrew, LLVM, clang, NDK or Xcode. FEX's front end runs natively on arm64 (macos/compat stands in for the Linux
# headers and system calls it names).
# Needs: Xcode or its Command Line Tools (the SDK), cmake, ninja, git, curl, python3 (with Pillow for the icon); fetch-deps-macos.sh fetches
# and builds the rest (LLVM 23.1.3 release tarball, zstd, SDL3, Dear ImGui, the Android NDK r29 for macOS).
# The app is signed ad hoc (codesign -s -), not notarized: on another Mac, the first start needs right click → Open (or
# `xattr -dr com.apple.quarantine <app>`).
set -euo pipefail
cd -- "$(dirname -- "$0")"
root=$PWD
test_pkgs=; compare=
while (( $# )); do
    case $1 in
        --test) test_pkgs=$2; shift 2 ;;
        --compare) compare=$2; shift 2 ;;
        *) echo "usage: $0 [--test <PKG folder> [--compare <libbbaot.so>]]" >&2; exit 2 ;;
    esac
done
[[ $(uname -s) == Darwin && $(uname -m) == arm64 ]] || { echo "package-macos.sh: needs an Apple Silicon Mac" >&2; exit 1; }
J=${BB_JOBS:-$(sysctl -n hw.ncpu)}
run() { nice -n 19 "$@"; }
VERSION=${BB_RECOMP_VERSION:-1.3.0}
APPNAME="Bloodborne Android Rebuild Tool"
ZIPNAME=bb-recomp-macos-arm64.zip
export MACOSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET:-14.4}

./fetch-deps-macos.sh
deps=${BB_RECOMP_DEPS:-$root/.local-deps}
llvm=$deps/llvm/x/LLVM-${BB_LLVM_VERSION:-23.1.3}-macOS-ARM64
prefix=$deps/deps/prefix-macos
imgui=$(ls -d "$deps"/imgui/imgui-* | head -n 1)
ndk=${BB_NDK:-$deps/android/ndk/android-ndk-r29}
nbin=$ndk/toolchains/llvm/prebuilt/darwin-x86_64
builtins=$(ls "$nbin"/lib/clang/*/lib/linux/libclang_rt.builtins-aarch64-android.a | head -n 1)

# 1. bb-recomp: window + command line
fexargs=()
[[ -n ${FEX_SOURCE_DIR:-} ]] && fexargs+=(-DFEX_SOURCE_DIR="$FEX_SOURCE_DIR")
mkdir -p out
run cmake -S . -B out/recomp-macos -G Ninja ${fexargs[@]+"${fexargs[@]}"} -DBB_RECOMP_BUNDLED=ON -DBB_RECOMP_VERSION="$VERSION" \
    -DBB_RECOMP_CC="$llvm/bin/clang" -DBB_RECOMP_CXX="$llvm/bin/clang++" -DCMAKE_OSX_DEPLOYMENT_TARGET="$MACOSX_DEPLOYMENT_TARGET" \
    -DBB_RECOMP_LLVM_DIR="$llvm/lib/cmake/llvm" -DBB_RECOMP_DEPS_PREFIX="$prefix" -DBB_RECOMP_SDL_PREFIX="$deps/sdl/prefix-macos" -DBB_RECOMP_IMGUI_DIR="$imgui" \
    -DBB_RECOMP_JOBS="$J" > out/recomp-macos-cfg.log 2>&1 || { tail -20 out/recomp-macos-cfg.log; exit 1; }
run cmake --build out/recomp-macos -j "$J"

# 2. ps4-pkg-tool (the app's PKG unpacker)
BB_LLVM_ROOT=$llvm shared/pkg-tool/build_pkg_tool.sh macos

# 3. the .app
dist=out/dist
app="$dist/$APPNAME.app"
rm -rf "$app"; mkdir -p "$app/Contents/MacOS" "$app/Contents/Resources/bb-recomp/variants" "$app/Contents/Resources/licenses"
share="$app/Contents/Resources/bb-recomp"
# the files the .so link needs, from the NDK: the same commands compile.cpp runs for a developer build
run "$nbin/bin/clang" --target=aarch64-linux-android30 -O2 -fPIC -fvisibility=hidden -fno-stack-protector -ffp-contract=off -c \
    helpers/bbr_x87_all.c -o "$share/helpers-aarch64.o"
# libbbcpu.so: a DT_NEEDED entry only (see compile.cpp: the real libbbcpu.so is already loaded when the game starts)
run "$nbin/bin/clang" --target=aarch64-linux-android30 -shared -nostdlib -Wl,-soname,libbbcpu.so -x c /dev/null -o "$share/libbbcpu-stub.so"
# libm.so: the NDK's own link stub (no code), so that the .so gets libm's DT_NEEDED and symbol versions exactly as a developer build
cp "$nbin/sysroot/usr/lib/aarch64-linux-android/30/libm.so" "$share/libm.so"
cp "$builtins" "$share/"
cp variants/*.txt "$share/variants/"   # the catalogs only (the generators stay in the source tree)
cp out/recomp-macos/bb-recomp "$app/Contents/MacOS/bb-recomp"
cp out/macos/pkg-tool/ps4-pkg-tool "$app/Contents/MacOS/ps4-pkg-tool"
"$llvm/bin/llvm-strip" "$app/Contents/MacOS/bb-recomp"
chmod 755 "$app/Contents/MacOS/"*
lic="$app/Contents/Resources/licenses"
fex=$(sed -n 's/^FEX_SOURCE_DIR:PATH=//p' out/recomp-macos/CMakeCache.txt)
[[ -f $fex/LICENSE ]] || fex=$root/out/recomp-fex   # the tree fetch-fex.sh prepared
cp "$fex/LICENSE" "$lic/FEX-MIT.txt"
cp "$deps/llvm/LICENSE.TXT" "$lic/LLVM-Apache-2.0-with-LLVM-exception.txt"
cp helpers/NOTICE "$lic/x87-helpers-NOTICE.txt"
cp LICENSE "$lic/GPL-3.0.txt"
cp NOTICES "$lic/NOTICES.txt"
cp ADDITIONAL_TERMS.md "$lic/ADDITIONAL_TERMS.md"
cp shared/pkg-tool/NOTICE "$lic/ps4-pkg-tool-NOTICE.txt"
cp docs/NATIVE_CPU.md "$share/NATIVE_CPU.md" 2>/dev/null || true
echo "$VERSION" > "$share/VERSION"
cat > "$app/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleName</key><string>$APPNAME</string>
  <key>CFBundleDisplayName</key><string>$APPNAME</string>
  <key>CFBundleIdentifier</key><string>io.github.ducvd89.bb-recomp</string>
  <key>CFBundleExecutable</key><string>bb-recomp</string>
  <key>CFBundleIconFile</key><string>AppIcon</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>CFBundleShortVersionString</key><string>$VERSION</string>
  <key>CFBundleVersion</key><string>$VERSION</string>
  <key>LSMinimumSystemVersion</key><string>$MACOSX_DEPLOYMENT_TARGET</string>
  <key>LSArchitecturePriority</key><array><string>arm64</string></array>
  <key>NSHighResolutionCapable</key><true/>
  <key>NSHumanReadableCopyright</key><string>GPL-3.0-or-later. Free software: if you paid for it, you were scammed.</string>
</dict>
</plist>
PLIST
# A simple generated icon (a chip: gold on dark, as the AppImage's); no game or console artwork.
iconset=out/macos/AppIcon.iconset
rm -rf "$iconset"; mkdir -p "$iconset"
python3 - "$iconset" <<'PY'
import sys
from PIL import Image, ImageDraw
S = 1024; im = Image.new('RGBA', (S, S), (0, 0, 0, 0)); d = ImageDraw.Draw(im); k = S / 256
gold, dark = (198, 173, 118, 255), (11, 12, 16, 255)
R = lambda *v: tuple(int(x * k) for x in v)
d.rounded_rectangle(R(8, 8, 248, 248), int(40 * k), fill=dark, outline=gold, width=int(6 * k))
d.rectangle(R(68, 68, 188, 188), fill=(30, 26, 18, 255), outline=gold, width=int(8 * k))
for i in range(5):
    t = 84 + i * 22
    for (a, b, c, e) in ((t, 36, t + 10, 68), (t, 188, t + 10, 220), (36, t, 68, t + 10), (188, t, 220, t + 10)): d.rectangle(R(a, b, c, e), fill=gold)
d.rectangle(R(100, 100, 156, 156), fill=gold)
for s in (16, 32, 128, 256, 512):
    im.resize((s, s), Image.LANCZOS).save(f"{sys.argv[1]}/icon_{s}x{s}.png")
    im.resize((2 * s, 2 * s), Image.LANCZOS).save(f"{sys.argv[1]}/icon_{s}x{s}@2x.png")
PY
iconutil -c icns "$iconset" -o "$app/Contents/Resources/AppIcon.icns"
codesign --force --deep --sign - "$app"
codesign --verify --deep --strict "$app"

# 4. the zip (ditto keeps the bundle's attributes and signature)
rm -f "$dist/$ZIPNAME"
(cd "$dist" && ditto -c -k --sequesterRsrc --keepParent "$APPNAME.app" "$ZIPNAME" && shasum -a 256 "$ZIPNAME" > "$ZIPNAME.sha256")
ls -l "$dist/$ZIPNAME"
nonsys=$(otool -L "$app/Contents/MacOS/bb-recomp" "$app/Contents/MacOS/ps4-pkg-tool" | awk '/^[ \t]/ && $1 !~ /^\/(usr\/lib|System)\// {print $1}' | sort -u | tr '\n' ' ')
echo "needs: macOS $(vtool -show-build "$app/Contents/MacOS/bb-recomp" | awk '/minos/{print $2}') or newer, arm64; libraries outside macOS: ${nonsys:-none}"

# 5. test in a clean environment (no LLVM, NDK or Homebrew on PATH)
if [[ -n $test_pkgs ]]; then
    t=$(mktemp -d "$root/out/bb-recomp-clean.XXXXXX")
    echo "clean-environment test in $t (env -i, PATH=/usr/bin:/bin)"
    ( cd "$t" && TIMEFORMAT='wall %R s, user %U s' && time env -i HOME="$t" PATH=/usr/bin:/bin "$root/$app/Contents/MacOS/bb-recomp" --pkgs "$test_pkgs" --out "$t/out" ) 2>&1 | tail -n 30
    so=$(ls "$t"/out/*/aot/libbbaot.so)
    [[ -f $so && -f ${so%.so}.json ]] || { echo "TEST FAILED: no libbbaot.so" >&2; exit 1; }
    if [[ -n $compare ]]; then
        if cmp -s "$so" "$compare"; then echo "TEST OK: byte-identical to $compare"
        else
            # A developer build links with the NDK's LLD 21, the release with LLD 23: the .comment string differs, which moves the (unloaded)
            # .symtab and e_shoff. Everything that is loaded must be identical.
            loadend=0
            while read -r off filesz; do e=$(( off + filesz )); (( e > loadend )) && loadend=$e; done < <("$llvm/bin/llvm-readelf" -lW "$so" | awk '$1=="LOAD"{print $2, $5}')
            if [[ -z $(cmp -l -n "$loadend" "$so" "$compare" | awk '$1<41||$1>48') ]]; then echo "TEST OK: the loaded part ($loadend bytes) is identical to $compare (only e_shoff and the .comment/.symtab tail differ: LLD version)"
            else echo "TEST FAILED: differs from $compare in the loaded part" >&2; exit 1; fi
        fi
    fi
    echo "kept: $t"
fi
