# Bloodborne Android Rebuild Tool

A PC program (Windows and Linux) that turns **your own** Bloodborne PKG files into a ready-to-copy
folder for the [Bloodborne Android port](https://github.com/ducvd89/Bloodborne-Android). It extracts
the game and its update, and **recompiles the game's x86-64 code ahead of time into native ARM64
code**, so the game runs as native code on the phone instead of through an x86 emulator (FEX).

> **Status: work in progress.** The recompiler works and the game reaches gameplay on an AYN Thor
> in native mode; the GUI, the Windows build and the first release are being finished. This
> repository receives the code when they are ready.

## Why you build it yourself

The recompiled code is a translation of the game's own program, so it belongs to the game's
copyright holders and can never be downloaded from here or anywhere else. The tool only works on a
game you own: it reads your PKG files on your PC and writes the result for your phone. Nothing from
the game is in this repository.

## License

GPL-3.0-or-later, with the additional terms in [ADDITIONAL_TERMS.md](ADDITIONAL_TERMS.md)
(attribution, marked modifications, no rights to the project's name or branding).

**This tool is free. Please don't sell it.** If you paid for it, you were scammed.
