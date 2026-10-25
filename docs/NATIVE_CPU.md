<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Native CPU (recompiled game code)

This is a guide for players (the last sections are for the app's developers). It explains what the native CPU is, how to build it on the phone or on your own PC, and how to get it onto your phone.

## What it is

Bloodborne is a PS4 game, so its code is written for an Intel-type (x86-64) processor. A phone has an ARM processor. Older versions of
the app translated the game's code **while you play** (with the FEX emulator), which cost CPU time and battery and caused small stutters
the first time the game ran a piece of code.

The **native CPU** does that translation **once, before you play**. The result is a file, `libbbaot.so`, which holds the whole
game's code already translated to ARM. Since app version 2.0.0 the game runs **only** from that file: there is no
emulator in the app any more, so the game does not start without it (the app explains how to get it). You can make the file in two ways:
**on the phone**, inside the app (since app 2.1.0 Beta; no PC needed), or **on a PC** with this tool.

**Versions.** Tool 1.2.0 writes native code with **ABI version 2**. It needs the app **2.1.0 Beta or newer**, and app 2.1.0 needs a library of ABI 2: a
library built by tool 1.1.0 or older (ABI 1) is refused, and the app asks for a rebuild. That is intended: those libraries were built before
fixes in the jump table scan, the granular entries and `rdtsc`, and a game that stopped with `guest trap (0x800d0b, 0x287188a)` was hitting them.

It is **new**: it has been tested on the AYN Thor (Snapdragon 8 Gen 2). If the game stops or misbehaves, please send a bug report
(Menu > Report a problem).

## Two ways to build it

- **On the phone, in the app** (app 2.1.0 Beta or newer): menu > Native CPU > **Build on this phone**. The app runs the same compiler as this tool on the
  game image it already has. No PC, no copying. It takes about **6 minutes** on a Snapdragon 8 Gen 2 with the charger connected (the phone gets hot), needs
  about **4 GB of free RAM** and **1 GB of free storage** while it works, and writes the same `libbbaot.so` and `libbbaot.json` a PC would (byte for byte
  the same library for the same game and tool version). See "The phone build" at the end for what the app runs.
- **On a PC** with this tool (below), then copy the `aot` folder to the phone.

## Why you build it yourself

`libbbaot.so` is made from the game's own program. That is Sony's and FromSoftware's code, so the file is a copy of the game in another
form. It **cannot be downloaded** and it cannot be shared: every player makes theirs on their own PC from their own copy of the game.
The tool that does it is free, and it runs on your machine: your game files are never uploaded anywhere.

The tool does everything in one go, from either of two sources:

| | **A. From PKG files** | **B. From an extracted game** |
|---|---|---|
| You give it | the folder with the base game PKG and the 1.09 update PKG | the game folder with 1.09 merged in (`eboot.bin`, `sce_module/`, `sce_sys/` ...), or its `eboot.bin` |
| It gives you | **one folder** to copy to the phone: the game with `aot/` inside | only the `aot/` folder (your game folder is never changed) |
| You copy | the game folder (the whole game) | `aot/` (about 125 MB), next to `eboot.bin` in the game folder on the phone |

Use B when the game is already extracted: a dump you made, the folder the app's "Install from PKG" made (copied to the PC), or the folder A
made earlier. It skips the PKG check and the unpacking, so it takes a few minutes.

## What you need

- **Your game**, either:
  - **A.** your Bloodborne PKG files, in one folder: the **base game** PKG and the **1.09 update** PKG (the same files the app's "Install from PKG"
    uses). Only "fake" PKGs (made by a PS4 dump or by the usual tools) can be read; PKGs encrypted for a real console cannot. Put only these PKG
    files in that folder; or
  - **B.** the extracted game with the 1.09 update merged in: the folder that holds `eboot.bin`, `sce_module/` and `sce_sys/` (and the game data).

  It must be 1.09 with the original `eboot.bin` (pre-patched 60 FPS eboots are refused). Tested with CUSA03173; the other 1.09 releases (CUSA00900, 00207, 00208, 00299, 01363, 03014, 03023, 03179) should work but are untested.
- A **PC**: Windows 10 (1903 or newer) or 11, 64-bit; or Linux with glibc 2.35 or newer (Ubuntu 22.04, Debian 12, Fedora 36, Arch ... or
  newer). The processor needs AVX2 (an Intel Haswell from 2013, AMD Zen, or newer).
- **Memory**: 8 GB of free RAM is enough, 16 GB or more is faster (the tool looks at how much is free and uses at most 60 % of it).
- **Disk space**: about 3 GB, plus for A the size of the game unpacked (about the size of the base game PKG). The tool tells you before it
  starts if there is not enough.
- **Time**: for A, unpacking the game depends on your disk (an SSD takes minutes, a slow hard disk much longer); building the native code takes
  about 2.5 minutes on a 24-thread PC and longer on a laptop with 4 cores.

## Windows

1. Download `bb-recomp-windows-x86_64.zip` from the Releases page of
   [github.com/ducvd89/Bloodborne-android-rebuild-tool](https://github.com/ducvd89/Bloodborne-android-rebuild-tool) and unzip it anywhere.
2. Double-click **`bb-recomp.exe`**. Windows may say the program is unknown (it is not signed with a paid certificate): click **More info**,
   then **Run anyway**.
3. Choose the source with its radio button (the row of the other one is grayed out):
   - **A. From PKG files**: press **Browse...** and choose the folder with the base game PKG and the 1.09 update PKG.
   - **B. From an extracted game**: press **Browse...** and choose the folder with `eboot.bin`, or press **eboot.bin...** and pick the file itself.
4. **Output folder**: press **Browse...** and choose where to put the result (an empty folder, or one that does not exist yet, on a disk
   with enough space).
5. Press **Build**. The bar shows the stage (for A: finding the PKGs, checking them for damage, extracting; for B: checking the game folder;
   then preparing, recompiling N/M shards, linking, copying) and the log shows the details. **Cancel** stops it. For A, before anything is unpacked, **every PKG is read once and checked against the
   checksums stored inside it** (this reads the whole file: a few minutes for the base game, and the window shows the speed). If a file is
   damaged the build stops there, names the file and what is wrong, and unpacks nothing.
6. When it says **Done**: for A the output folder holds the game folder (`CUSA03173` or the number of your release) and a `README.txt`; for B
   it holds the `aot` folder. **Open the folder** shows it.

Folder names with accents or other alphabets are fine. If the tool says a file or folder cannot be created, choose a shorter path
(for example `D:\bloodborne`).

## Linux

1. Download `Bloodborne-Android-Rebuild-Tool-x86_64.AppImage` from the same Releases page.
2. Make it runnable (`chmod +x Bloodborne-Android-Rebuild-Tool-x86_64.AppImage`, or right-click > Properties > Allow executing) and
   double-click it, or run it in a terminal. It opens the same window as on Windows: steps 3 to 6 above.
3. The AppImage needs FUSE 2 (the package `libfuse2` or `fuse2`). If your system does not have it, run it like this instead:
   `./Bloodborne-Android-Rebuild-Tool-x86_64.AppImage --appimage-extract-and-run`
4. The **Browse...** buttons use your desktop's file chooser (xdg-desktop-portal). If none appears, type or paste the folder path into the box.

## From the command line

The same program runs without a window when you give it folders (this works on Windows and Linux, in a terminal):

```
bb-recomp --pkgs <folder with the PKGs> --out <output folder>                  (A)
bb-recomp --game <game folder or its eboot.bin> --out <output folder>          (B)
```

On Linux the AppImage takes the same arguments (`./Bloodborne-Android-Rebuild-Tool-x86_64.AppImage --pkgs ... --out ...`). Add `--cli` first if
you want to be sure no window opens. Other options: `--jobs N` (number of worker threads), `--mem-fraction 0.4` (use less of the free memory),
`--keep-work` (keep the temporary folder `<out>/.bbwork`), `--help-player` (all options). `--game` writes only `libbbaot.so` and
`libbbaot.json` into `<output folder>/aot/` and never changes the game folder; without `--out` it writes to `./bb-recomp-out`.

The exit code is 0 when it finished and 1 when it did not; the reason is the last line it printed.

## What you get

**A. From PKG files:**

```
<output folder>/
    README.txt                what to do next
    bb-recomp.log             the statistics of the build
    CUSA03173/                the game folder: Bloodborne 1.09 unpacked from your PKGs, laid out as the app expects
        eboot.bin, sce_sys/, sce_module/, dvdroot_ps4/ ...
        aot/
            libbbaot.so       the game's code for the phone (about 125 MB)
            libbbaot.json     what it was made from (game, image hash, tool version, size and checksum)
```

**B. From an extracted game:**

```
<output folder>/
    bb-recomp.log             the statistics of the build
    aot/
        libbbaot.so           the game's code for the phone (about 125 MB)
        libbbaot.json         what it was made from
```

For A you can delete the PKG files afterwards if you like; the output folder is the whole game. These files come from your game: keep them for
yourself.

## On the phone

You need the app **2.1.0 Beta or newer** (ABI 2; an app 2.0.0 Beta 1 needs a library built by tool 1.1.0).

1. Copy to the phone's storage or an SD card (a USB cable in file transfer mode, `adb push`, or a card reader all work):
   - **A:** the game folder (`CUSA03173`, with `aot` inside; copying the whole output folder works too). It is a big copy (the whole game).
   - **B:** the `aot` folder, into the game folder on the phone, next to `eboot.bin`. (Or put it into the extracted folder on the PC first, if
     you are about to copy that folder to the phone anyway.)
2. In the app open the menu > **Game folder** and choose that folder.
3. When the game starts, the app sees `aot/libbbaot.so` and `aot/libbbaot.json` in the game folder, checks them (the game, the version, the
   build's checksum and ABI version), **copies them into its own files** (a program cannot be run from shared storage, so the app needs its own
   copy). It copies again only when the files in the game folder change. There is nothing to switch on.
4. **Restart the game once** if it was already running. Menu > **Native CPU (recompiled)** shows the state: **Ready**, not installed, or
   built for another game version (with the game version, number of units and tool version).

If the game is already on the phone (for example from **Install from PKG**), you do not need to copy the game again: build from the files you
have on the PC (A or B) and copy only the `aot` folder into the game's folder on the phone (it goes next to `eboot.bin`), then start the game.

You can also import the two files by hand: Menu > Native CPU (recompiled) > **Import libbbaot.so...**, then select `libbbaot.so` and
`libbbaot.json` together (or one after the other). The same checks apply.

## When to build it again

- When the app tells you the library uses another **ABI version**. A new app release can change how the game and the library talk to
  each other: build again with the tool release the app names (app 2.1.0 Beta: tool 1.2.0, ABI 2), or let the app build it on the phone.
- **When a new tool version fixes something that was wrong in the code it wrote.** 1.2.0 is such a version: a library built by 1.1.0 or older can stop the
  game with `guest trap (0x800d0b, 0x287188a)`; the ABI number was raised so that the app asks for the rebuild.
- When the game's files change (a different game release or update). The menu says **not for this game**.

You do not need to build it again for a new version of the app that has the same ABI version, or for a new phone (copy the same game
folder to it).

## Troubleshooting

**"There are no .pkg files in ..."** - the folder you chose has no files ending in `.pkg`. Choose the folder that holds them directly (the tool
does not look in sub-folders). If you have an extracted game instead, choose **From an extracted game** (`--game`).

**"this does not look like a Bloodborne game folder. Missing in ...:"** - (B) the folder you chose does not hold `eboot.bin`,
`sce_module/libc.prx`, `sce_module/libSceFios2.prx` and `sce_sys/param.sfo`. Choose the folder that holds them directly; the message lists
sub-folders that look right ("Did you mean one of these?").

**"... is for CUSAxxxxx, which isn't a Bloodborne release bbport supports"** - one of the PKGs in the folder is another game, or DLC. Take
everything but the Bloodborne game PKG and its update out of the folder.

**"The base game PKG is missing" / "Add the 1.09 update PKG"** - the folder needs the game PKG (the big one) and the update PKG for version
1.09. The tool lists what it found.

**"This PKG is encrypted for a retail PS4 and can't be read"** - only fake PKGs work. PKGs downloaded from the PlayStation Store for a real
console are encrypted and cannot be used.

**"<file> is damaged: ..." / "This PKG file is incomplete: it should be N bytes and has M"** - the copy or download did not finish, or a byte was
changed (a bad cable, card or disk). The message names the file and the part that fails (the file is shorter than it should be, or the header,
the body or the game data does not match its checksum). Download or dump **that file** again, or copy it again, and run the tool again. Nothing
had been unpacked yet.

**"... is the update to version X, older than the base game"** - the update PKG does not belong to this base game. Use the 1.09 update for your
game.

**"Not enough free space"** - free some space on the disk of the output folder, or choose another disk.

**"this game is not supported. Found CUSAxxxxx version 01.0x ..."** - the folder does not hold the 1.09 game. The message says what is
missing (the update, or an `eboot.bin` that is not the 1.09 one).

**The game stops with "guest trap (0x800d0b, 0x287188a)"** - the library was built by tool 1.1.0 or older. Build it again with tool 1.2.0 (or let the app 2.1.0 Beta
build it on the phone).

**"cannot map the image at the lift base ..." (the tool, or the app's build on the phone)** - the tool maps the game's image at a fixed address while it
analyses it (default `0x2b00000000`, a place that also exists on phones whose kernel has a 39-bit address space) and found it taken. It then tries two
other places by itself; if all fail the message says so. Close other programs and start again; with the command line `--base <hex>` (a multiple of 4 GB
below `0x8000000000`) picks another place. The result does not depend on the place.

**It stops with "out of memory", or the PC freezes** - close other programs, or run it from the command line with fewer workers and less
memory: `--jobs 4 --mem-fraction 0.4`. It is slower but needs less memory.

**"Illegal instruction" at start** - the processor has no AVX2 (it is older than 2013).

**The AppImage does not start** - see the FUSE note above: `--appimage-extract-and-run`.

**The app says "The native CPU files in the game folder were not used: ..."** - the message gives the reason: the library was made for another
game version, by a tool of another version (ABI), or it was damaged while being copied (copy the folder again; a cable or card that drops
bytes shows up here).

**The menu says "Installed, but unusable" or "not for this game"** - build again from the game files (PKGs or folder) of the game you actually play, or choose the
right game folder.

**The app reports a missing native entry** ("native code is missing an entry at +0x..."; the details are in `logs/aot-misses.log` in the app's folder) - the
library does not cover a place the game jumped to. Copy `aot-misses.log` to the PC and rebuild with it: `bb-recomp --pkgs <PKG folder> --out <folder> --extra-entries aot-misses.log`
(or `bb-recomp --game <game folder> --out <folder> --extra-entries aot-misses.log`). Every address in the log becomes an entry of the new library. Please also send the log with a report.

**The game hangs or crashes** - send a report (Menu > Report a problem); it includes the logs that show where it stopped.

## The phone build (for the app's developers)

Bloodborne Android 2.1.0 Beta ships the tool's command line build as `libbbrecomp.so` and runs it to make `libbbaot.so` on the phone. `arm64/package-android.sh`
makes the files (`out/android/`: the binary, `bundle/`, `variants/`, `licenses/`, `VERSION`, `README-ANDROID.md`, and the tarball
`bb-recomp-android-aarch64.tar.xz`). The binary is a static AArch64 executable (glibc, LLVM 23.1.3 with only the AArch64 back end, LLD in the process, 58 MB stripped); it needs no
NDK, no clang, no Python, no `eboot.elf`, no `link.json` and no `.prx` folder. Same sources as the PC tool: the library it writes is byte-identical to the PC's for the same
game, tool version and `--shards` (checked: x86-64 build, the AArch64 build under qemu, and the AYN Thor).

### Command line

Paths absolute, without spaces or quotes. The working directory does not matter.

```
libbbrecomp.so --boot <data>/boot-linked.bin --game-dir <game folder> \
    --bundle-dir <bundle dir> --variants <variants dir> \
    --so <work>/libbbaot.so --stats <work>/compile.txt \
    --jobs <n> --shards 64 -O1 --no-nice --progress --resume --clean-work
```

| Input | |
|---|---|
| `--boot` | the game image the app builds (`bbport/data/boot-linked.bin`, BBPROBE5, what `bb-prepare` writes). The image hash in the library is taken from it |
| `--game-dir` | a game folder. Read: `eboot.bin`, `sce_module/libc.prx`, `sce_module/libSceFios2.prx` (the ELF program headers, for the place of each module's `.eh_frame_hdr`; the files are SELF files and are read as they are; module order is that of the boot image: libc, then libSceFios2) and `sce_sys/param.sfo` (TITLE_ID, APP_VER, TITLE, CONTENT_ID for the json). Nothing else, nothing is changed. A folder with just these four files works. The game check (1.09, unpatched eboot) is the app's: the tool trusts the boot image |
| `--bundle-dir`, `--variants` | `out/android/bundle` and `out/android/variants`, copied out of the APK to files |
| `--so` | the folder of this path is the work folder (must exist, about 1 GB free while it works, 130 MB at the end) |
| `--jobs <n>` | worker threads. Memory is about 0.5 GB + 0.8 GB per job (Thor: `--jobs 4` peak 3.8 GB, 5 min 55 s with the charger connected). With `--shards 64` fixed the result does not depend on `--jobs` or on the available memory; without `--shards` the tool changes the shard count to fit the memory and the bytes of the library change with it |
| `--no-nice` | the tool otherwise lowers its own priority to 19 |

Outputs (in the work folder): `libbbaot.so`; `libbbaot.json` (written by the tool after it has read the `.so` back and checked the ABI version, the image hash against the boot image and the unit table; the
json is the format of player mode: `format` 1, `kind`, `game_id`, `game_version`, `content_id`, `game_title`, `image_hash`, `image_size`, `abi_version` 2, `abi_flags`, `unit_count`, `function_count`, `tool`, `llvm`, `created`,
`so_size`, `so_sha256`); the `--stats` file (the summary). Its name is the name of the `.so` with `.json`; `--json <file>` names another. `--clean-work` deletes the objects (`libbbaot.so.o.NNN.o`,
their `.stat` files, `libbbaot.table.o`, `libbbaot.helpers.o`, `libbbaot.var*.o`, `libbbaot.so.libbbcpu-stub.so`) after the success. The app verifies the `.so` against the json (`so_size`, `so_sha256`) as it does for a PC build.

### Progress, exit status, stop and resume

With `--progress` every run ends with `BBR-DONE <exit status>` and, on a failure, `BBR-ERROR <message>` (one line) just before it. The machine lines are flushed and appear between the human lines of stdout:

```
BBR-PROGRESS <stage> <done> <total>
```

| Stage | `done` / `total` | Share of the time |
|---|---|---|
| `scan` | 0/1 when it starts, 1/1 when the image, the function list and the landing pads are known | 1 % |
| `discover` | functions examined of all (the search for code without an FDE; it has later rounds that report nothing) | 2 % |
| `lift` | code bytes lifted of the code to compile (the pass that learns the unit set) | 2 % |
| `compile` | work done of the whole compile, in code bytes x 100 (about 3 % of a shard's work is lifting, the rest optimiser and code generator; a finished shard counts all of it, a reused one too) | 93 % |
| `variants` | functions with variants done of all | 1 % |
| `link` | 0/3 the unit table, 1/3 the x87 helpers, 2/3 the link, 3/3 done (lines of a stage that come within a quarter of a second of the last are left out, the first and the last of every stage always appear) | 1 % |
| `json` | 0/1, 1/1 | - |
| `done` | 1/1, printed just before `BBR-DONE 0` | - |

`done` only grows within a stage. A stage can be absent (no `variants` without a catalog). The shares are from the CPU time of a PC build (7 s of analysis, 1638 s of compile, a few serial seconds for variants and link); they hold on the phone too, so one
bar can use these weights and take the ETA from the finished fraction over time (the `compile` lines come about every 250 ms on a PC, about 140 in a whole build).

| Exit status | |
|---|---|
| 0 | done: `libbbaot.so` and `libbbaot.json` are complete and checked |
| 1 | failed; the message is in `BBR-ERROR` and on stderr (for example `cannot map the image ...`, a read error, link failure) |
| 2 | bad command line |
| 143 / 130 | stopped by SIGTERM / SIGINT |
| 137, other 128+n | killed from outside (for example by the low memory killer): no `BBR-DONE` line |

**Stop.** SIGTERM (or SIGINT) stops the analysis and the compile at the next function; a shard that is in the optimiser or the code generator is finished first (about a second, never more than a few); after 8 seconds the
tool ends itself anyway. Finished shards stay in the work folder; nothing half written is left that a restart would trust (objects are written under a temporary name and renamed, the `.stat` file comes last).

**Resume.** With `--resume` every finished shard leaves `libbbaot.so.o.NNN.o` and `libbbaot.so.o.NNN.o.stat` in the work folder. Run the same command again in the same folder (after a stop, a kill, a reboot) and the tool reuses every shard whose `.stat`
key still matches (a hash over the tool version, the lift base, the image, every option that changes the code, the unit set, the function list, the hook declarations and the shard's functions) and whose object has the recorded size. The scan, discovery and unit-set passes, the
variants, the table, the link and the json run again (seconds on a PC, well under a minute on a phone). The library is byte-identical to an uninterrupted build (checked: an interrupted and resumed build equals a fresh one). Start another game version, another tool version or other
options in an empty work folder, or delete the old one. A `libbbaot.so` that exists after a kill may be half linked: only `BBR-DONE 0` makes it a result.

### The lift base

The tool maps the game image at a fixed host address while it analyses it (the "lift base"; `--base <hex>` changes it). The default is now `0x2b00000000` for every host (it was `0x3a7700000000`, which does not exist on kernels with a 39-bit address space, such as the Thor's:
`CONFIG_ARM64_VA_BITS=39`, user space ends at `0x8000000000`). The base must be chosen so that no plain constant of the game lands in `[base, base + image size)`: the lowering relocates every 64-bit constant in that window as an image address. For Bloodborne 1.09 the check is the number of constants the lowering relocates:
1,866,280 with the old base and with `0x2b00000000` (also `0x3d00000000`, `0x4700000000`, `0x4a00000000`, `0x5100000000`), but 1,866,287 with `0x2000000000`, 1,866,283 with `0x2300000000`, 1,866,282 with `0x2600000000` (plain constants of the game inside the window): those bases would change the code. With the default, the old base and the fallbacks the code is byte-identical: the -O0 objects for all of the bases above that have the same count, and the -O1 libraries of the old base, the new default, the phone-style command line and the two fallbacks (same sha256). The base is `hi << 32` (the low word of an entry-point offset is the vaddr) with `hi < 0x80`.
When the address range cannot be mapped the tool tries `0x3d00000000` and `0x4700000000` (checked the same way) and then stops with `cannot map the image (N MB) at the lift base 0x...: <reason> ...` (`BBR-ERROR` too); `--base` given on the command line is never replaced.

## For developers

- Tool sources, build and package scripts: the root of the tool's repository (see its `README.md`). `package.sh` builds the
  AppImage, `package-windows.sh` the Windows zip (cross-compiled with llvm-mingw).
- How the translated code runs and what the library contains: `docs/RECOMP_ABI.md`, `docs/STATIC_RECOMPILE_PLAN.md`.
- The `.json` is written by `src/aot_json.cpp` (player mode and `--game-dir`); the app reads it in `NativeCpu.java` (checks) and `NativeCpuDialog.java` (menu).
  Since app 2.0.0 the native code is required: the app has no switch for it and no x86 emulator to fall back to.
- The tool is free software (GPL-3.0-or-later). If you paid for it, you were scammed.
