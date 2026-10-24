#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Lowering regressions found on the AYN Thor (the Bloodborne-Android repository, docs/AOT_DEVICE_STATUS.md), checked on the game's own functions.
# Needs the game image of the 1.09 release CUSA03173: the eboot ELF, boot-linked.bin and the dump's sce_module folder. `bb-recomp --game <dump> --out O --keep-work`
# leaves the first two in O/.bbwork/eboot.elf and O/.bbwork/data/boot-linked.bin.
#  1. +0x221b124 `movabs rax, 0x200000002` (two packed counts) must stay that constant, not be relocated as an image address
#     (the old lift base 0x200000000 made it image + 2).
#  2. +0x21b8420 `L: cmp eax, [r13 + 0x18]; jg L` (a spin-wait with no store) must load each iteration: a volatile load.
# usage: BB_ELF=<eboot.elf> BB_BOOT=<boot-linked.bin> tests/run_recomp_device_regressions.sh <sce_module dir>   (defaults: out/eboot.elf, out/boot-linked.bin)
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
tool=${BB_RECOMP:-out/recomp-build/bb-recomp}
elf=${BB_ELF:-out/eboot.elf}; boot=${BB_BOOT:-out/boot-linked.bin}
prx=${1:-${BB_SCE_MODULE:-}}
[[ -x $tool && -f $elf && -f $boot && -d $prx ]] || { echo "device regressions: needs $tool, $elf, $boot and the sce_module folder as the argument (or BB_SCE_MODULE)" >&2; exit 2; }
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
args=(--elf "$elf" --boot "$boot" --prx-dir "$prx" --target aarch64)
fail=0
nice -n 19 "$tool" "${args[@]}" --emit-ll 0x221b124 "$tmp/const.ll" >/dev/null 2>&1
if grep -q 'i64 8589934594' "$tmp/const.ll" && ! grep -q 'add nuw nsw i64 %imgbase, 2$' "$tmp/const.ll"; then
    echo "PASS packed constant 0x200000002 kept"
else echo "FAIL packed constant 0x200000002 relocated (+0x221b124)"; fail=1; fi
nice -n 19 "$tool" "${args[@]}" --emit-ll 0x21b8420 "$tmp/spin.ll" >/dev/null 2>&1
if grep -q 'load volatile i32' "$tmp/spin.ll"; then
    echo "PASS spin-wait load volatile (+0x21b8420)"
else echo "FAIL spin-wait load not volatile (+0x21b8420)"; fail=1; fi
#  3. +0x20b55b0 (a game panic path: `call 0x20df920 ; movl $0xdeadba, 0x0`): the units must carry null_pointer_is_valid. Without it LLVM deletes the store to address 0
#     as undefined behaviour and the call-return block before it, and the return from the print function became an unwind ("AOT: native code is missing an entry at +0x20b56d2").
nice -n 19 "$tool" "${args[@]}" --emit-ll 0x20b55b0 "$tmp/panic.ll" >/dev/null 2>&1
if grep -q 'store i32 14593466, ptr null' "$tmp/panic.ll" && grep -q 'null_pointer_is_valid' "$tmp/panic.ll"; then
    echo "PASS null store after a call kept valid (null_pointer_is_valid, +0x20b55b0)"
else echo "FAIL a store to address 0 after a call is undefined behaviour for LLVM (+0x20b55b0)"; fail=1; fi
exit $fail
