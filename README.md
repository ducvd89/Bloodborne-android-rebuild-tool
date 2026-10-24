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

## Downloads

**Coming soon** on the [Releases page](https://github.com/ducvd89/Bloodborne-android-rebuild-tool/releases):
`Bloodborne-Android-Rebuild-Tool-x86_64.AppImage` (Linux, glibc 2.35 or newer) and
`bb-recomp-windows-x86_64.zip` (Windows 10 1903 or newer, 64-bit). There are no releases yet; until
then build it from source (below). The packages need no LLVM, clang, NDK or lld on your machine.

## Usage

You need your Bloodborne PKG files in one folder: the **base game** and the **1.09 update**
(fake PKGs, as the app's "Install from PKG" reads), a PC with an AVX2 processor, about 8 GB of free
RAM (16 GB or more is faster), and the size of the game in free disk space plus about 3 GB. The
complete guide for players, with the troubleshooting list, is [docs/NATIVE_CPU.md](docs/NATIVE_CPU.md).

**The window.** Run `bb-recomp.exe` (Windows) or the AppImage (Linux: `chmod +x` it first, and if
FUSE 2 is missing run it with `--appimage-extract-and-run`). Choose the PKG folder and an output
folder, press **Build**. Every PKG is checked for damage before anything is unpacked. When it says
**Done**, the output folder holds the game folder (`CUSA03173` or the number of your release) with
`aot/libbbaot.so` and `aot/libbbaot.json` inside, and a `README.txt`.

**The command line** (the same program, the same result):

```
bb-recomp --pkgs <folder with the PKGs> --out <output folder>
bb-recomp --game <extracted game folder> --out <output folder>   # a dump with eboot.bin instead of PKGs: writes only libbbaot.so and .json
bb-recomp --help-player                                          # all options (--jobs, --mem-fraction, --keep-work, --variants ...)
```

The exit code is 0 when it finished. Building the native code takes about 2 minutes on a 12-core PC.

**Copying to the phone.** Copy the game folder (with `aot` inside) to the phone's storage or SD card
(USB cable, `adb push` or a card reader), then in the app open Menu > Game folder and choose it. The
app finds `aot/libbbaot.so`, checks it (game, version, checksum, ABI), copies it into its own files
and switches the native CPU on; restart the game once. If the game is already installed on the
phone, copy only the `aot` folder next to its `eboot.bin`. You can also import the two files by hand
in Menu > Native CPU (recompiled) > Import.

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

`bb-recomp --so` (writing the AArch64 library) uses the Android NDK r29: `./fetch-ndk.sh`, then `--ndk
.local-deps/android/ndk/android-ndk-r29` (or set `BB_NDK`). The game image builder's sources are in
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
mkdir -p .local-deps/nasm/dl && cd .local-deps/nasm/dl && curl -fsSLO https://www.nasm.us/pub/nasm/releasebuilds/2.16.03/nasm-2.16.03.tar.xz
tar xf nasm-2.16.03.tar.xz -C .. && mkdir -p ../build && cd ../build
nice -n 19 ../nasm-2.16.03/configure --prefix=$PWD/../install && nice -n 19 make -j22 install
```

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
