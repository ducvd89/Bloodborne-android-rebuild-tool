<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# bb-recomp for Android (what the app ships)

The Bloodborne Android app (2.1.0 Beta and newer) recompiles the game's x86-64 code to ARM64 on the phone with this program, the command line
build of the Bloodborne Android Rebuild Tool (version in `VERSION`). It makes the same `libbbaot.so` a PC makes (byte for byte, for the same game
and tool version). The full contract is in `docs/NATIVE_CPU.md` of the tool's repository ("The phone build"); this is the short form.

## Files

| File | Use |
|---|---|
| `libbbrecomp.so` | the program: an AArch64 executable built with the Android NDK (r29, API 30, bionic: a dynamically linked PIE with `/system/bin/linker64` as interpreter, needing only `libc.so`, `libm.so` and `libdl.so`; libc++, LLVM and LLD are linked in; it is not a shared library). It only makes system calls that bionic makes, so the app's seccomp filter lets it run, and a crash leaves a debuggerd tombstone. Ship it as `jniLibs/arm64-v8a/libbbrecomp.so` with native libraries extracted to disk (`android:extractNativeLibs="true"` / `useLegacyPackaging`) and run it from `nativeLibraryDir` |
| `bundle/` | the four files the link of `libbbaot.so` needs (`helpers-aarch64.o`, `libbbcpu-stub.so`, `libm.so`, `libclang_rt.builtins-aarch64-android.a`); `--bundle-dir`. They are read as files: copy them out of the APK (assets) to the app's storage once |
| `variants/*.txt` | the variant catalog (patch groups and hook sites of the port); `--variants <dir>`. Copy out of the APK the same way |
| `licenses/` | the licenses of what is linked in (ship them with the app) |
| `VERSION` | tool version, git commit, ABI version of the libraries it writes, how the program was built (`target` line) |

## Command line

All paths absolute and without spaces or quotes (the program splits some internal commands on spaces).

```
libbbrecomp.so --boot <data>/boot-linked.bin --game-dir <game folder> \
    --bundle-dir <bundle dir> --variants <variants dir> \
    --so <work>/libbbaot.so --stats <work>/compile.txt \
    --jobs <n> --shards 64 -O1 --no-nice --progress --resume --clean-work
```

- `--boot`: the game image the app already builds (`bbport/data/boot-linked.bin`, the BBPROBE5 file bb-prepare writes).
- `--game-dir`: the game folder. Only four files are read: `eboot.bin`, `sce_module/libc.prx`, `sce_module/libSceFios2.prx` (their ELF program headers; the
  whole files are read once) and `sce_sys/param.sfo` (game id, version, title, content id for `libbbaot.json`). A folder that holds just these four
  files, copied to the app's storage, does as well. Nothing in it is changed. There is no `eboot.elf`, `link.json`, Python or `--prx-dir` involved.
- `--so`: the work folder is the folder of this path (it must exist, with ~1 GB free): the objects and the result go there. Outputs when it succeeds:
  `<work>/libbbaot.so`, `<work>/libbbaot.json` (written by the program, as the PC tool writes it) and the `--stats` file; `--clean-work` then deletes the objects.
- `--jobs`: worker threads. Memory: about 0.5 GB + 0.8 GB per job (measured on the AYN Thor: `--jobs 4`: 3.8 GB peak, 5 min 55 s). `--shards 64` is fixed so that the
  result does not depend on `--jobs` or on the memory.
- `--progress`: lines on stdout, see below. `--no-nice`: the program otherwise lowers its own priority. `--resume`: see below.
- The lift base is chosen by the program (it fits 39-bit address spaces); no option is needed.

## Progress, exit status

stdout carries human lines and these machine lines (each flushed): `BBR-PROGRESS <stage> <done> <total>` with stage one of `scan`, `discover`, `lift`, `compile`,
`variants`, `link`, `json`, `done` (`done` counts only grow inside a stage; `compile` is by code bytes weighted by work: done/total is the fraction of the whole
compile), `BBR-ERROR <message>` (a failure, one line) and always last `BBR-DONE <exit status>`. Exit status: 0 done; 1 failed (see `BBR-ERROR`); 2 bad command line;
143 / 130 stopped by SIGTERM / SIGINT (finished shards stay in the work folder); 137 and other signals: killed from outside (low memory).
Suggested weights of the stages for one bar: scan 1 %, discover 2 %, lift 2 %, compile 93 %, variants 1 %, link 1 % (from the CPU time of a PC build: 7 s of analysis against 1638 s of compile; the phone is slower per core but the ratio holds).

SIGTERM stops at the next function boundary (about a second; a shard in the optimiser finishes first, at most a few seconds). Run the same command again with the
same work folder after a stop or a kill: the shards that finished are reused (`--resume`; checked by a key over the tool version, the image, the options and the shard),
the analysis stages and the link run again (a few seconds on a PC, well under a minute on the phone). Use a fresh work folder for another game version or tool version.
The result counts only when `BBR-DONE 0` came: a `libbbaot.so` that exists after a kill may be half-linked. Delete the work folder when you do not want to resume.
