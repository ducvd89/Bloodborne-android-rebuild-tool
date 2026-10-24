# Kept in sync with the Android app

These sources (the game folder check and the game image builder) exist twice: here, where the PC tool builds the game image the native code is
compiled from, and in the Bloodborne Android app repository (https://github.com/ducvd89/Bloodborne-Android, `tools/android/prepare/`), where the app
runs the same code on the phone (`bb-prepare`). The image hash in `libbbaot.so` depends on them being the same code: **change both, or neither.**

With the app repository checked out next to this one (or its path given):

    bash shared/check_prepare_sync.sh ../Bloodborne-Android      # diffs shared/prepare with the app's tools/android/prepare (ignoring SPDX header lines)

The app repository has this tool as its `tools/recomp` submodule and the same check in `tools/android/check_prepare_sync.sh`.
