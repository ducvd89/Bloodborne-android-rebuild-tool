#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Check of phase 6 on the real image (needs the eboot ELF, boot-linked.bin and the dump's sce_module folder, see run_recomp_device_regressions.sh; no code is generated, ~2 s):
# the three guest addresses the Thor logged as misses in a 13-minute session must be covered:
#   +0x20       the static-initialiser runner at the start (no FDE)           -> a unit entry
#   +0x210ebc0  CSChrThread's loop head (85 k misses)                         -> a case of a resolved jump table
#   +0x23a9c9c  EzWorkPool's switch case (746 misses)                         -> a case of a resolved jump table
# plus a list file in the misses-log format through --extra-entries (an address that is a unit entry because the log said so), and the table dump.
# usage: BB_ELF=<eboot.elf> BB_BOOT=<boot-linked.bin> BB_SCE_MODULE=<sce_module dir> tests/run_aot_entries_check.sh   (defaults: out/eboot.elf, out/boot-linked.bin)
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
tool=${BB_RECOMP:-out/recomp-build/bb-recomp}
elf=${BB_ELF:-out/eboot.elf}; boot=${BB_BOOT:-out/boot-linked.bin}
sce=${BB_SCE_MODULE:-}
[[ -x $tool && -f $elf && -f $boot && -d $sce ]] || { echo "aot entries check: skipped (needs $tool, $elf, $boot and BB_SCE_MODULE)"; exit 0; }
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
args=(--elf "$elf" --boot "$boot" --prx-dir "$sce" --units-only --no-nice)
out=$("$tool" "${args[@]}" --check-entries 20,210ebc0,23a9c9c --dump-units "$tmp/units.txt")
echo "$out" | grep -E '^\+0x|not covered'
echo "$out" | grep -q '^+0x20: unit entry'
echo "$out" | grep -q '^+0x210ebc0: case of the jump table'
echo "$out" | grep -q '^+0x23a9c9c: case of the jump table'
echo "$out" | grep -q '^0 of 3 addresses are not covered'
grep -q '^unit 0x20 ' "$tmp/units.txt"
# A misses log: an address in the middle of a function becomes a unit entry; one in no function a pseudo-function.
printf '# bb aot misses\nmiss +0x23a9d00 kind=jump thread=T tid=1 rsp=0x0 backtrace=+0x1\nmiss +0x10 kind=call thread=T tid=1 rsp=0x0 backtrace=\n' > "$tmp/misses.log"
out=$("$tool" "${args[@]}" --extra-entries "$tmp/misses.log" --check-entries 23a9d00 2>&1)
echo "$out" | grep -E 'extra entries|^\+0x'
echo "$out" | grep -q '^+0x23a9d00: unit entry'
echo "aot entries check: ok"
