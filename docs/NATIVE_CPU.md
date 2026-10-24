<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Native CPU (recompiled game code)

This is a guide for players. It explains what the native CPU is, how to build it on your own PC, and how to get it onto your phone.

## What it is

Bloodborne is a PS4 game, so its code is written for an Intel-type (x86-64) processor. A phone has an ARM processor. Older versions of
the app translated the game's code **while you play** (with the FEX emulator), which cost CPU time and battery and caused small stutters
the first time the game ran a piece of code.

The **native CPU** does that translation **once, before you play**, on your PC. The result is a file, `libbbaot.so`, which holds the whole
game's code already translated to ARM. Since app version 2.0.0 the game runs **only** from that file: there is no
emulator in the app any more, so the game does not start without it (the app explains how to get it).

It is **new**: it has been tested on the AYN Thor (Snapdragon 8 Gen 2). If the game stops or misbehaves, please send a bug report
(Menu > Report a problem).

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
  about 2 minutes on a 12-core PC and longer on a laptop with 4 cores.

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

You need the app **2.0.0 Beta 1 or newer**.

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
  each other: download the tool of the same release and build again.
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

## For developers

- Tool sources, build and package scripts: the root of the tool's repository (see its `README.md`). `package.sh` builds the
  AppImage, `package-windows.sh` the Windows zip (cross-compiled with llvm-mingw).
- How the translated code runs and what the library contains: `docs/RECOMP_ABI.md`, `docs/STATIC_RECOMPILE_PLAN.md`.
- The `.json` is written by `src/player.cpp`; the app reads it in `NativeCpu.java` (checks) and `NativeCpuDialog.java` (menu).
  Since app 2.0.0 the native code is required: the app has no switch for it and no x86 emulator to fall back to.
- The tool is free software (GPL-3.0-or-later). If you paid for it, you were scammed.
