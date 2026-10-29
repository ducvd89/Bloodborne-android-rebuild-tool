<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Bloodborne Android Rebuild Tool

The [Bloodborne Android port](https://github.com/ducvd89/Bloodborne-Android) runs the game only as **native ARM64 code**: the game's x86-64 code is recompiled ahead of
time from **your own** copy of the game, once, into a file (`libbbaot.so`). This tool does that recompiling. You can run it in two places:

- **on the phone**, inside the app (Bloodborne Android **2.1.0 Beta** or newer): no PC needed;
- **on a PC** (Windows, Linux, or a Mac with Apple Silicon), from your PKG files or an already extracted game, and copy the result to the phone.

> **Version 1.2.0** writes native code with **ABI version 2**. It needs the app **2.1.0 Beta or newer**, and app 2.1.0 needs a library from tool 1.2.0. Libraries built by
> tool 1.1.0 or older (ABI 1) are refused by the app and must be rebuilt: they contain a bug that can stop the game with `guest trap (0x800d0b, 0x287188a)`.
> Tested on an AYN Thor (Snapdragon 8 Gen 2).
>
> **Version 1.2.1** changes only the phone build (the app 2.1.1 carries it): it is now an Android (NDK, bionic) program, so it also runs on Android 16
> and 17, where the security policy stopped the 1.2.0 phone build at once. The code it writes is the same as 1.2.0's, byte for byte; on a PC keep using 1.2.0.
> Tested on an AYN Thor (Android 13) and an OPPO Find N5 (Snapdragon 8 Elite, Android 16).
>
> **Version 1.3.0** adds hook sites for the guest-code fixes of the upstream bbport 0.7 (cloth stiffness above 30 FPS, a guard for the GX occlusion query crash, play time above 30 FPS,
> the hit collector cap, the character id guard) and **delta rebuilds**: the build files are kept, only the parts that changed are rebuilt, and a build manifest written by a PC build lets the
> phone check its parts against it. The library is still **ABI 2**. Libraries from 1.2.x still run, but without the new fixes: rebuild to get them (the app 2.2.0 asks for it once).

## Why you build it yourself

The recompiled code is a translation of the game's own program, so it belongs to the game's
copyright holders and can never be downloaded from here or anywhere else. The tool only works on a
game you own: it reads your PKG files or game folder and writes the result for your phone. Nothing from
the game is in this repository (no PKG, no executable, no extracted file, no recompiled library,
ever), and nothing is uploaded: everything runs on your own device. The files the tool writes come from
your game: keep them for yourself and **never share them**.

## Two ways to get the native code

| | **A. On the phone** (in the app) | **B. On a PC** (this tool) |
|---|---|---|
| **You need** | Bloodborne Android **2.1.0 Beta** or newer, with the game installed in it. | The tool, and either the PKG files or an extracted game (see below). Then a copy to the phone. |
| **Time** | About **6 minutes** on a Snapdragon 8 Gen 2 with the **charger connected**; the phone gets hot. | A few minutes on a PC (2.5 minutes on a 24-thread PC); more for unpacking PKGs. |
| **Resources** | About **4 GB of free RAM** and **1 GB of free storage** while it works. | 8 GB of free RAM, about 3 GB of disk (plus the PKG size). |
| **Result** | The app uses it by itself. | An `aot` folder (about 125 MB) to copy next to `eboot.bin` on the phone. |

### A. On the phone

When the app has no native code for its version it shows **Native code needed**: choose **Build on this phone** (later rebuilds: menu > **Native CPU** > **Rebuild on this phone**), and keep the app open with the charger connected until it is done. The app shows a progress bar and an estimate of the time
left. The phone gets warm and slows down if it overheats: take it out of its case and do not use it for other things. If the app is stopped or the phone restarts, start the build again:
the parts that were finished are kept. The tool inside the app is the same program as the PC tool (the same compiler, the same output: the library is the same file byte for byte).

### B. On a PC

You build the native code on a PC from your own game files and copy it to the phone. Pick the way that matches the files you have:

| | **B1. From PKG files** | **B2. From an extracted game** |
|---|---|---|
| **You have** | The base game PKG and the 1.09 update PKG (fake PKGs, the same files the app's "Install from PKG" uses), together in one folder with nothing else in it. | A game folder with the 1.09 update merged in: `eboot.bin`, `sce_module/`, `sce_sys/` and the rest (for example the folder the app's "Install from PKG" made, or a dump you extracted yourself). |
| **The tool writes** | The whole game folder (`CUSA03173`) with `aot/` inside, and a README.txt. | Only `aot/` (`libbbaot.so`, `libbbaot.json`). Your game folder is never changed. |
| **You copy to the phone** | The game folder (a large copy). | The `aot/` folder (about 125 MB), next to `eboot.bin` in the game folder there. |
| **Time** | 15-30 minutes, mostly unpacking and copying. | A few minutes. |

**Game already on the phone?** Build from whichever files you still have on the PC, then copy only the `aot/` folder next to `eboot.bin` on the phone.

#### What you need

| | |
|---|---|
| **Game files** | B1 or B2 above: Bloodborne **1.09** with the original `eboot.bin` (pre-patched 60 FPS eboots are refused). Tested with **CUSA03173**; other releases of 1.09 should work but are untested. |
| **PC** | Windows 10 (1903+) / 11, 64-bit, or Linux with glibc 2.35+ (Ubuntu 22.04, Debian 12, Fedora 36, Arch or newer): a processor with AVX2 (Intel 2013+ or any AMD Ryzen). Or a Mac with Apple Silicon (M1 or newer) and macOS 14.4+. |
| **Memory / disk** | 8 GB free RAM (16 GB+ is faster). Free disk space: about 3 GB, plus the size of the base game PKG for B1. |
| **Phone** | The Bloodborne Android app **2.1.0 Beta or newer**. |

#### Step 1: Get the tool

Download it from the [latest release](https://github.com/ducvd89/Bloodborne-android-rebuild-tool/releases/latest) (or build it from source, see
[Building from source](#building-from-source)); check the file against its `.sha256`:

- **Windows:** `bb-recomp-windows-x86_64.zip`. Unzip it anywhere and double-click `bb-recomp.exe`. If Windows
  SmartScreen warns that the app is unknown, click **More info → Run anyway** (the tool is not signed with a
  paid certificate).
- **macOS (Apple Silicon):** `bb-recomp-macos-arm64.zip`. Unzip it and move **Bloodborne Android Rebuild Tool** to Applications.
  The app is not notarized: the first time, **right-click it → Open → Open** (or run
  `xattr -dr com.apple.quarantine "/Applications/Bloodborne Android Rebuild Tool.app"`). Intel Macs are not supported.
  Built from the [`macos`](https://github.com/ducvd89/Bloodborne-android-rebuild-tool/tree/macos) branch, which also has its build instructions.
- **Linux:** `Bloodborne-Android-Rebuild-Tool-x86_64.AppImage`. Make it executable
  (`chmod +x Bloodborne-Android-Rebuild-Tool-x86_64.AppImage`) and double-click it. If nothing happens, your
  system lacks FUSE 2: run it from a terminal with `./Bloodborne-Android-Rebuild-Tool-x86_64.AppImage --appimage-extract-and-run`.

It is free. If you paid for it, you were scammed.

#### Step 2: Build

1. Choose the source. Each has its own radio button; the row of the other one is grayed out.
   - **From PKG files → Browse…**: the folder with the base game PKG and the 1.09 update PKG.
   - **From an extracted game → Browse…** (the folder with `eboot.bin`) or **eboot.bin…** (pick the file itself).
2. **Output folder → Browse…**: an empty folder on a disk with enough space (a short path such as
   `D:\bloodborne` or `~/bloodborne` is best).
3. Press **Build** and wait. The progress bar shows each stage:

   | Stage | What happens | Time |
   |---|---|---|
   | Checking PKGs (B1) | Every PKG is read once and checked against its own checksums. A damaged file stops the build **before anything is unpacked**, with the file name and what is wrong. | A few minutes |
   | Extracting (B1) | The base game and the update are unpacked and merged. | Minutes (depends on the disk) |
   | Checking the game folder (B2) | `eboot.bin`, `sce_module` and `sce_sys` must be there; the tool names what is missing. | Instant |
   | Preparing | The game image is built, the same way the app does it, and checked to be 1.09. | Seconds |
   | Recompiling | The game's x86-64 code is translated to ARM64. | About 2.5 min on a 24-thread PC, longer on a laptop |
   | Linking, checking, copying | The result is written and checked against the game image. | Seconds |

4. When it says **Done**, the output folder contains:

   ```
   B1. From PKG files                          B2. From an extracted game
   <output folder>/                             <output folder>/
       README.txt                                   bb-recomp.log
       bb-recomp.log                                aot/
       CUSA03173/       ← the whole game                libbbaot.so    ← native ARM64 code (~125 MB)
           eboot.bin, sce_sys/, sce_module/ …           libbbaot.json  ← what it was built from
           aot/
               libbbaot.so
               libbbaot.json
   ```

   **Open the folder** opens it.

Prefer a terminal? The same program runs without a window (on Linux, pass the same arguments to the AppImage; on macOS the program is
`"/Applications/Bloodborne Android Rebuild Tool.app/Contents/MacOS/bb-recomp"`):

```
bb-recomp --pkgs <PKG folder> --out <output folder>                   # B1
bb-recomp --game <game folder or its eboot.bin> --out <output folder>   # B2
```

#### Step 3: Copy to the phone

Use a USB cable (file transfer mode), a card reader, or `adb push`.

- **B1:** copy the game folder (`CUSA03173`, with `aot` inside) to the phone's storage or SD card.
- **B2:** copy the `aot` folder into the game folder on the phone, next to `eboot.bin`. (Or put it into the
  extracted folder on the PC first, if you are about to copy that folder to the phone anyway.)

#### Step 4: Choose the folder in the app

1. Open the app's menu → **Game folder**, and choose the game folder (the one with `eboot.bin` and `aot`).
2. Start the game. The app finds `aot/libbbaot.so`, checks that it matches your game (game build, ABI
   version, checksum) and copies it into its own storage. That's all: there is nothing to switch on.
3. Menu → **Native CPU** shows the state (**Ready**, not installed, or built for another game
   version) and can also import `libbbaot.so` and `libbbaot.json` by hand.

If the game stops with a message about a **missing native entry**, the app saved the address in
`aot-misses.log`: rebuild on the PC with `--extra-entries aot-misses.log` (see Troubleshooting) and send a report
(menu → **Report a problem**).

## When you must build again

- **The app changes the native code format (ABI version).** The app then says the library has another ABI version. Build again with the tool release the app names
  (app 2.1.0 Beta: tool 1.2.0, ABI 2), or let the app do it (A).
- **A new tool version fixes the code it writes.** 1.2.0 is one: a library built by 1.1.0 or older can stop the game with `guest trap (0x800d0b, 0x287188a)`
  (a jump table scan and a probe fix). The ABI number was raised so that the app asks for the rebuild.
- **The game files change** (another release or update): the menu says **not for this game**.

A new phone does not need a new build: copy the same game folder to it.

## Troubleshooting

| Message | Fix |
|---|---|
| The game stops with `guest trap (0x800d0b, 0x287188a)` | The library was built by tool 1.1.0 or older. Build it again with 1.2.0 (on the phone in app 2.1.0 Beta, or on a PC). |
| *cannot map the image (N MB) at the lift base 0x…* | The tool analyses the game at a fixed address range (`0x2b00000000`, which exists on phones with a 39-bit address space too) and found it taken; it tries two other ranges by itself. Close other programs and start again; on a PC command line `--base <hex>` (a multiple of 4 GB below `0x8000000000`) chooses another. The result does not depend on the range. |
| Out of memory / the build is killed / the phone freezes (A) | Close other apps, keep the charger connected and start again: finished parts are kept. About 4 GB of free RAM are needed; a phone with less may not manage. |
| Out of memory / the PC freezes (B) | Close other programs, or use the terminal with `--jobs 4 --mem-fraction 0.4`. |
| *There are no .pkg files in …* | Choose the folder that directly contains the PKGs (sub-folders are not searched). Have an extracted game instead? Use **From an extracted game**. |
| *… is damaged* / *This PKG file is incomplete* | That file was not copied or downloaded completely. Get **that file** again and rebuild. Nothing was unpacked. |
| *… isn't a Bloodborne release* / DLC found | Remove everything except the Bloodborne base game PKG and its update from the folder. |
| *Add the 1.09 update PKG* | The update PKG is missing or is another version. |
| *encrypted for a retail PS4* | Only fake PKGs can be read. |
| *This does not look like a Bloodborne game folder* | Choose the folder that directly holds `eboot.bin`, `sce_module` and `sce_sys` (the message lists what is missing and sub-folders that look right). |
| *This game is not supported* | The game is not 1.09, the update is not merged in, or the `eboot.bin` is a patched one. Use the original 1.09 files. |
| *Not enough free space* | Free space on the output disk or choose another disk. |
| *Illegal instruction* | The processor has no AVX2. |
| App: *not for this game* / *other ABI version* | Rebuild from the game files you play, with the tool release matching your app (2.1.0 Beta: 1.2.0). |
| App: a missing native entry ("native code is missing an entry at +0x…", details in `logs/aot-misses.log`) | The library does not cover a place the game jumped to. Copy `aot-misses.log` to the PC and rebuild with `--extra-entries aot-misses.log` added to your command (`--pkgs …` or `--game …`). Every logged address becomes an entry of the new library. |

The full reference, with every message and the details of each step, is [docs/NATIVE_CPU.md](docs/NATIVE_CPU.md); the phone build's command line and progress format for the app
are in its "The phone build" section.

## Building from source

### Linux

The developer build (the command line tool and the test runners). It needs clang, LLVM 23 with the
AArch64 and X86 targets, cmake, ninja, git, python3 and zlib:

```
git clone --recursive https://github.com/ducvd89/Bloodborne-android-rebuild-tool
cd Bloodborne-android-rebuild-tool
cmake -S . -B out/recomp-build -G Ninja
nice -n 19 cmake --build out/recomp-build -j 22
# -> out/recomp-build/bb-recomp, bb-recomp-asmtest, bb-recomp-difftest
```

The first configure fetches FEX-Emu 2609.1 (the commit `third_party/FEX` pins, shallow, with only
the submodules the front end needs, into `out/recomp-fex`; this needs network once and does not
modify any checkout). A recursive clone leaves `third_party/FEX` empty on purpose: FEX's own
submodules include about 150 MB of test binaries that the tool does not use. To use your own FEX
checkout pass `-DFEX_GIT_DIR=<path>`, for a ready tree `-DFEX_SOURCE_DIR=<path>`.

`bb-recomp --so` and `--game`/`--pkgs` (which write the AArch64 library) use the Android NDK r29 in the developer
build: run `./fetch-ndk.sh` once (it unpacks into `.local-deps/android/ndk/android-ndk-r29`, where the tool looks; `--ndk` or
`BB_NDK` name another). The packages carry what they need from it. The game image builder's sources are in
`shared/prepare` (the Android app builds the same files).

Tests (the assembly tests need nasm, see [below](#nasm-for-the-assembly-tests)):

```
out/recomp-build/bb-recomp-asmtest --fex out/recomp-fex   # about 1755/1760 pass (the rest are x87 corner cases of the reduced-precision mode)
out/recomp-build/bb-recomp-asmtest --hook-selftest
out/recomp-build/bb-recomp-asmtest --variant-selftest
out/recomp-build/bb-recomp-asmtest --resume-selftest
tests/recomp-pkgs/run.sh out/recomp-build/bb-recomp       # the PKG step on synthetic PKGs and a stand-in PKG tool
```

The self-contained release, the AppImage (static LLVM and LLD, SDL3 and Dear ImGui, built against an
Ubuntu 22.04 sysroot so that it runs on glibc 2.35 and newer):

```
./package.sh        # -> out/dist/Bloodborne-Android-Rebuild-Tool-x86_64.AppImage
```

It downloads and builds its dependencies into `.local-deps` (`fetch-deps.sh`: the LLVM 23.1.3 release
tarball, an Ubuntu sysroot, zlib, zstd, SDL3, Dear ImGui; `fetch-ndk.sh`: the NDK;
`shared/pkg-tool/fetch-ps4-pkg-tools.sh`: ps4-pkg-tools at 45baeda, which
`shared/pkg-tool/build_pkg_tool.sh` patches with `shared/pkg-tool/patches` and builds). Plan on a few GB
of downloads and about half an hour the first time. Everything runs at `nice -n 19`; `BB_JOBS` sets the
number of jobs (22 by default).

### Cross-building for Windows

On Linux, `./package-windows.sh` cross-compiles the Windows program with llvm-mingw (static: nothing to
install), builds LLVM and LLD for mingw from source (about 12 minutes on 22 cores) and writes
`out/dist/bb-recomp-windows-x86_64.zip`. There is no native Windows build script.

### The Android build (what the app ships)

`arm64/build.sh android` builds the command line tool with the Android NDK r29 as a bionic AArch64 executable (a dynamically linked PIE that needs only libc, libm and libdl; it builds LLVM 23.1.3 + LLD
for the phone from source first, like the Windows build does). `arm64/build.sh aarch64` is the older static glibc build (it needs the LLVM Linux-ARM64 tarball and an arm64 sysroot, which it fetches into `.local-deps`;
Android 16 and newer kill it with SIGSYS). `arm64/package-android.sh` packs it with the link files, the variant catalog and the licenses into `out/android/` and `out/dist/bb-recomp-android-aarch64.tar.xz`. The app runs
it as `libbbrecomp.so`; how is described in [docs/NATIVE_CPU.md](docs/NATIVE_CPU.md) ("The phone build") and `arm64/README-ANDROID.md`.

### nasm for the assembly tests

```
mkdir -p .local-deps/nasm && cd .local-deps/nasm && curl -fsSLO https://www.nasm.us/pub/nasm/releasebuilds/2.16.03/nasm-2.16.03.tar.xz
tar xf nasm-2.16.03.tar.xz && cd nasm-2.16.03
nice -n 19 ./configure --prefix=$PWD/../install && nice -n 19 make -j22 install && cd ../../..
```

(`bb-recomp-asmtest` finds it at `.local-deps/nasm/install/bin/nasm`; `-DBB_RECOMP_NASM=<path>` or `--nasm <path>` name another.)

### Regenerating the variant catalogs

`variants/*.txt` (the patch groups and hook sites that the recompiler compiles as variants, so that the
phone's runtime can still apply the port's patches and hooks to native code) are generated and **checked
in**. They hold addresses, patch bytes and hook declarations, no game code. Building the tool does not
regenerate them. The generators read the sources of the Android port and the game's own files, so
regenerating or checking a catalog needs the **main project checked out** and the eboot and linked image
that the tool makes from your PKGs (`--keep-work` leaves them in `<out>/.bbwork`):

```
git clone https://github.com/ducvd89/Bloodborne-Android ../Bloodborne-Android
variants/gen_patch_parts.py --main-repo ../Bloodborne-Android --eboot <eboot.elf> --check variants/patch-parts.txt
variants/gen_hooks_shim.py  --main-repo ../Bloodborne-Android --check variants/hooks-shim.txt
variants/gen_patchers.py    --main-repo ../Bloodborne-Android --eboot <eboot.elf> --boot <boot-linked.bin> --check
variants/gen_bbsc.py        --main-repo ../Bloodborne-Android --eboot <eboot.elf> --check
```

`BB_MAIN_REPO` can replace `--main-repo`; leave out `--check` to rewrite a catalog.

## Repository layout

| Path | What |
|---|---|
| `src/` | the tool: FEX lifter, IR census, LLVM lowering, ARM64 object and `.so` writer, the player program (`player.cpp`, `aot_json.cpp`, `pkgs.cpp`, `pkgverify.cpp`), the window (`gui.cpp`), the test runners |
| `arm64/` | the Android NDK build (and the static glibc one) and the packing of what the app ships (`build.sh`, `package-android.sh`, `README-ANDROID.md`) |
| `helpers/` | the x87 helpers the lowered code calls (adapted from FEX, MIT; SoftFloat, BSD-3-Clause) |
| `variants/` | the generated variant and hook catalogs, and their generators |
| `windows/` | support files of the Windows (llvm-mingw) build |
| `shared/prepare/` | the game image builder (`bb-prepare`), shared with the Android app; compiled into the tool |
| `shared/pkg-tool/` | the build script and the patches for `ps4-pkg-tool`, and the script that fetches its source |
| `third_party/FEX` | the pin of FEX-Emu 2609.1 (a submodule; `fetch-fex.sh` fetches it) |
| `tests/recomp-pkgs/` | synthetic PKGs and a stand-in PKG tool for the PKG step |
| `docs/` | the guide for players ([NATIVE_CPU.md](docs/NATIVE_CPU.md)), the compiler reference ([COMPILER.md](docs/COMPILER.md)) and the design documents ([RECOMP_ABI.md](docs/RECOMP_ABI.md), [RECOMP_HOOKS.md](docs/RECOMP_HOOKS.md), [STATIC_RECOMPILE_PLAN.md](docs/STATIC_RECOMPILE_PLAN.md)) |
| `NOTICES`, `ADDITIONAL_TERMS.md`, `LICENSE` | third-party notices, the additional terms, the license |

## The Android port

The app that loads the result: [github.com/ducvd89/Bloodborne-Android](https://github.com/ducvd89/Bloodborne-Android).
It reads `libbbaot.json` and the `aot/` folder of the game folder; the format is in `src/aot_json.cpp`
(`"format": 1`), and the library's ABI (version 2 since tool 1.2.0) is in [docs/RECOMP_ABI.md](docs/RECOMP_ABI.md) (`bbr_abi_version`,
checked by the tool and by the app). The app 2.1.0 Beta also runs the tool's Android build (`arm64/package-android.sh`, [docs/NATIVE_CPU.md](docs/NATIVE_CPU.md)) on the phone.

## License

GPL-3.0-or-later ([LICENSE](LICENSE)), with the additional terms in [ADDITIONAL_TERMS.md](ADDITIONAL_TERMS.md)
(attribution, marked modifications, no rights to the project's name or branding); the About box of the
tool shows them. Third-party components keep their own licenses: FEXCore (MIT), SoftFloat (BSD-3-Clause),
LLVM (Apache-2.0 with LLVM exceptions), Dear ImGui and mimalloc (MIT), SDL3 and zlib (zlib), zstd
(BSD-3-Clause), ps4-pkg-tools (GPL-2.0-or-later); the files of `shared/prepare` are GPL-2.0-or-later,
as in the Android app. See [NOTICES](NOTICES). Every file has an SPDX header.

**This tool is free. Please don't sell it.** If you paid for it, you were scammed. (That is a request,
not a license condition; the license conditions are in ADDITIONAL_TERMS.md.)

Not affiliated with Sony Interactive Entertainment or FromSoftware.
