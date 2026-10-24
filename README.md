<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Bloodborne Android Rebuild Tool

A PC program (Windows and Linux) that turns **your own** Bloodborne PKG files into a ready-to-copy
folder for the [Bloodborne Android port](https://github.com/ducvd89/Bloodborne-Android). It extracts
the game and its update, and **recompiles the game's x86-64 code ahead of time into native ARM64
code**, so the game runs as native code on the phone instead of through an x86 emulator (FEX).

> **Status: experimental.** The recompiler works and the game reaches gameplay on an AYN Thor in
> native mode; it has been seen to hang before the title screen once in a while. The window, the
> command line and the Linux and Windows packages are done; the first release is coming.

## Why you build it yourself

The recompiled code is a translation of the game's own program, so it belongs to the game's
copyright holders and can never be downloaded from here or anywhere else. The tool only works on a
game you own: it reads your PKG files on your PC and writes the result for your phone. Nothing from
the game is in this repository (no PKG, no executable, no extracted file, no recompiled library,
ever), and nothing is uploaded: everything runs on your machine. The files the tool writes come from
your game: keep them for yourself and do not share them.

## How to use it

From your Bloodborne PKG files to the game running as native ARM64 code on your phone, in about 15–30
minutes (mostly copying files).

### What you need

| | |
|---|---|
| **Game files** | Your own Bloodborne **base game PKG** and the **1.09 update PKG** (fake PKGs, the same files the app's "Install from PKG" uses), together in one folder with nothing else in it. Tested with **CUSA03173**; other releases of 1.09 should work but are untested. |
| **PC** | Windows 10 (1903+) / 11, 64-bit, or Linux with glibc 2.35+ (Ubuntu 22.04, Debian 12, Fedora 36, Arch or newer). A processor with AVX2 (Intel 2013+ or any AMD Ryzen). |
| **Memory / disk** | 8 GB free RAM (16 GB+ is faster). Free disk space: about the size of the base game PKG plus 3 GB. |
| **Phone** | The Bloodborne Android app in a version with the **Native CPU** menu (coming with the next app release), and room for the game folder. |

### Step 1: Get the tool

Download it from the [Releases page](https://github.com/ducvd89/Bloodborne-android-rebuild-tool/releases)
(**not published yet**: until the first release, build it from source, see [Building from source](#building-from-source)):

- **Windows:** `bb-recomp-windows-x86_64.zip`. Unzip it anywhere.
- **Linux:** `Bloodborne-Android-Rebuild-Tool-x86_64.AppImage`. Make it executable
  (`chmod +x Bloodborne-Android-Rebuild-Tool-x86_64.AppImage`).

It is free. If you paid for it, you were scammed.

### Step 2: Build

1. Start the tool:
   - **Windows:** double-click `bb-recomp.exe`. If Windows SmartScreen warns that the app is unknown, click
     **More info → Run anyway** (the tool is not signed with a paid certificate).
   - **Linux:** double-click the AppImage. If nothing happens, your system lacks FUSE 2: run it from a terminal
     with `./Bloodborne-Android-Rebuild-Tool-x86_64.AppImage --appimage-extract-and-run`.
2. **Game PKG folder → Browse…**: choose the folder with the base game PKG and the 1.09 update PKG.
3. **Output folder → Browse…**: choose an empty folder on a disk with enough space (a short path such as
   `D:\bloodborne` or `~/bloodborne` is best).
4. Press **Build** and wait. The progress bar shows each stage:

   | Stage | What happens | Time |
   |---|---|---|
   | Checking PKGs | Every PKG is read once and checked against its own checksums. A damaged file stops the build **before anything is unpacked**, with the file name and what is wrong. | A few minutes |
   | Extracting | The base game and the update are unpacked and merged. | Minutes (depends on the disk) |
   | Preparing | The game image is built, the same way the app does it. | Seconds |
   | Recompiling | The game's x86-64 code is translated to ARM64. | About 1–2 min on a 12-core PC, longer on a laptop |
   | Linking, copying | The result is written. | Seconds |

5. When it says **Done**, the output folder contains:

   ```
   <output folder>/
       README.txt
       bb-recomp.log
       CUSA03173/              ← the whole game, ready for the phone
           eboot.bin, sce_sys/, sce_module/, dvdroot_ps4/ …
           aot/
               libbbaot.so     ← the game's code as native ARM64 (about 125 MB)
               libbbaot.json   ← what it was built from (checked by the app)
   ```

Prefer a terminal? The same program runs without a window:
`bb-recomp --pkgs <PKG folder> --out <output folder>` (on Linux, pass the same arguments to the AppImage).

### Step 3: Copy to the phone

Copy the game folder (`CUSA03173`, with `aot` inside) to the phone's storage or SD card, using a USB cable
(file transfer mode), a card reader, or `adb push`. It is a large copy.

**Game already installed on the phone?** You don't need to copy the game again. Copy only the `aot` folder
into the phone's game folder, next to `eboot.bin`.

### Step 4: Turn it on in the app

1. Open the app's menu → **Game folder**, and choose the folder you copied.
2. Start the game. The app finds `aot/libbbaot.so`, checks that it matches your game, copies it into its own
   storage and switches **Native CPU** on. Restart the game once if it was already running.
3. Menu → **Native CPU (recompiled)** shows the state (installed, matches this game, on or off), and has
   the **On/Off** switch. It can also import `libbbaot.so` and `libbbaot.json` by hand.

If the game hangs or crashes with it on, switch it **off** there, and send a report (menu → **Report a
problem**).

### When to build again

- After a new app release that says the library has another **ABI version**: download the matching tool
  release and build again.
- When you change the game files (another release or update): the menu says **not for this game**.

A new phone does not need a new build: copy the same game folder to it.

### Common problems

| Message | Fix |
|---|---|
| *There are no .pkg files in …* | Choose the folder that directly contains the PKGs (sub-folders are not searched). |
| *… is damaged* / *This PKG file is incomplete* | That file was not copied or downloaded completely. Get **that file** again and rebuild. Nothing was unpacked. |
| *… isn't a Bloodborne release* / DLC found | Remove everything except the Bloodborne base game PKG and its update from the folder. |
| *Add the 1.09 update PKG* | The update PKG is missing or is another version. |
| *encrypted for a retail PS4* | Only fake PKGs can be read. |
| *Not enough free space* | Free space on the output disk or choose another disk. |
| Out of memory / the PC freezes | Close other programs, or use the terminal with `--jobs 4 --mem-fraction 0.4`. |
| *Illegal instruction* | The processor has no AVX2. |
| App: *not for this game* / *other ABI version* | Rebuild from the PKGs of the game you play, with the tool release matching your app. |

The full reference, with every message and the details of each step, is [docs/NATIVE_CPU.md](docs/NATIVE_CPU.md).

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
| `src/` | the tool: FEX lifter, IR census, LLVM lowering, ARM64 object and `.so` writer, the player program (`player.cpp`, `pkgs.cpp`, `pkgverify.cpp`), the window (`gui.cpp`), the test runners |
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
It reads `libbbaot.json` and the `aot/` folder of the game folder; the format is in `src/player.cpp`
(`"format": 1`), and the library's ABI is in [docs/RECOMP_ABI.md](docs/RECOMP_ABI.md) (`bbr_abi_version`,
checked by the tool and by the app).

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
