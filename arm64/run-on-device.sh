#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Runs the static AArch64 bb-recomp (arm64/build.sh) on an Android phone over adb, to time the recompile there. Run it on the PC with the phone connected
# (USB debugging on; ANDROID_SERIAL=<serial> when more than one device is attached). Nothing is installed: everything lives in /data/local/tmp/bbr,
# the runs go through `adb shell` as the shell user, and the work files (the sample .so and its objects, ~25 MB) are deleted after each run.
#   run-on-device.sh                     --plan-only for the whole game (default jobs, then --jobs 8 and 4), then the 1/17 sample (--every 17, ~10.7k
#                                        functions) compiled and linked to a .so with --jobs 8 and --jobs 4
# Environment: BB_GAME (default /home/cid/BloodbornePC: out/eboot.elf, out/boot-linked.bin, out/link.json, game/CUSA03173/sce_module), BB_BIN (the binary:
# default bb-recomp next to this script), BB_EVERY (17), BB_JOBS ("8 4"), BB_COOLDOWN (seconds between runs, 45), BB_CLEAN_ALL=1 (also delete the
# binary and the inputs from the phone at the end: ~270 MB), BB_DEV_DIR (/data/local/tmp/bbr).
# The tool runs with --no-nice (it lowers its own priority to 19 otherwise). Results: results-<time>/ next to this script (one <label>.log with the
# tool's summary, <label>.device.txt with MemAvailable, temperatures and frequencies, summary.txt).
set -euo pipefail
HERE=$(cd -- "$(dirname -- "$0")" && pwd)
GAME=${BB_GAME:-/home/cid/BloodbornePC}
BIN=${BB_BIN:-$HERE/bb-recomp}
BUNDLE=${BB_BUNDLE:-$HERE/bundle}
EVERY=${BB_EVERY:-17}
JOBS_LIST=${BB_JOBS:-"8 4"}
COOLDOWN=${BB_COOLDOWN:-45}
DEV=${BB_DEV_DIR:-/data/local/tmp/bbr}
ELF=$GAME/out/eboot.elf
BOOT=$GAME/out/boot-linked.bin
LINK=$GAME/out/link.json
PRX=$GAME/game/CUSA03173/sce_module
PRX_FILES=(libc.prx libSceFios2.prx)   # the modules link.json names (the boot image holds the eboot plus these two)
ADB=(adb)

for f in "$BIN" "$ELF" "$BOOT" "$LINK" "$HERE/device-run-one.sh" "$BUNDLE/helpers-aarch64.o" "$BUNDLE/libbbcpu-stub.so" "$BUNDLE/libm.so" "$BUNDLE/libclang_rt.builtins-aarch64-android.a"; do
    [[ -f $f ]] || { echo "missing $f" >&2; exit 1; }
done
for p in "${PRX_FILES[@]}"; do [[ -f $PRX/$p ]] || { echo "missing $PRX/$p" >&2; exit 1; }; done
command -v adb >/dev/null || { echo "adb not found" >&2; exit 1; }
state=$("${ADB[@]}" get-state 2>&1 || true)
[[ $state == device ]] || { echo "no usable adb device (adb get-state: $state); set ANDROID_SERIAL when several are attached" >&2; exit 1; }

OUT=$HERE/results-$(date +%Y%m%d-%H%M%S)
mkdir -p "$OUT"
log() { echo "[$(date +%H:%M:%S)] $*"; }
dsh() { "${ADB[@]}" shell "$@"; }

# push FILE to DEVPATH unless the phone already has a file of the same size
push() {
    local src=$1 dst=$2 want have
    want=$(stat -c %s "$src")
    have=$(dsh "stat -c %s '$dst' 2>/dev/null || echo 0" | tr -d '\r')
    if [[ $have == "$want" ]]; then log "have $dst ($want bytes)"; else log "push $src -> $dst ($want bytes)"; "${ADB[@]}" push "$src" "$dst" >/dev/null; fi
}

log "device info"
{
    echo "model: $(dsh getprop ro.product.model | tr -d '\r')  soc: $(dsh 'getprop ro.soc.model; getprop ro.board.platform' | tr -d '\r' | tr '\n' ' ')" || true
    echo "android: $(dsh getprop ro.build.version.release | tr -d '\r')  sdk: $(dsh getprop ro.build.version.sdk | tr -d '\r')  kernel: $(dsh uname -a | tr -d '\r')" || true
    echo "cpus: $(dsh 'grep -c ^processor /proc/cpuinfo' | tr -d '\r')" || true
    dsh 'for p in /sys/devices/system/cpu/cpufreq/policy*; do echo "$p cpus=$(cat $p/related_cpus 2>/dev/null) max=$(cat $p/cpuinfo_max_freq 2>/dev/null) cur=$(cat $p/scaling_cur_freq 2>/dev/null) gov=$(cat $p/scaling_governor 2>/dev/null)"; done' | tr -d '\r' || true
    dsh 'grep -E "MemTotal|MemAvailable|SwapTotal" /proc/meminfo; df -h /data/local/tmp | tail -1; dumpsys battery 2>/dev/null | grep -E "level|temperature|powered|status"' | tr -d '\r' || true
} | tee "$OUT/device-info.txt"

log "push to $DEV"
dsh "mkdir -p '$DEV/bundle' '$DEV/in/sce_module'"
push "$BIN" "$DEV/bb-recomp"
for f in helpers-aarch64.o libbbcpu-stub.so libm.so libclang_rt.builtins-aarch64-android.a; do push "$BUNDLE/$f" "$DEV/bundle/$f"; done
push "$ELF" "$DEV/in/eboot.elf"
push "$BOOT" "$DEV/in/boot-linked.bin"
push "$LINK" "$DEV/in/link.json"
for p in "${PRX_FILES[@]}"; do push "$PRX/$p" "$DEV/in/sce_module/$p"; done
"${ADB[@]}" push "$HERE/device-run-one.sh" "$DEV/device-run-one.sh" >/dev/null
dsh "chmod 755 '$DEV/bb-recomp' '$DEV/device-run-one.sh'"

log "smoke test"
dsh "cd '$DEV' && ./bb-recomp --version" | tr -d '\r' | tee "$OUT/version.txt"

# the runs happen in $DEV/work-<label>, one level below the inputs
COMMON="--boot ../in/boot-linked.bin --elf ../in/eboot.elf --prx-dir ../in/sce_module --bundle-dir ../bundle --no-nice"

# run <label> <extra tool args...>: one run in a fresh work dir, results pulled, work dir deleted
run() {
    local label=$1; shift
    log "run $label: bb-recomp $COMMON $*"
    local rc=0
    dsh "cd '$DEV' && rm -rf work-$label && mkdir work-$label && cd work-$label && sh ../device-run-one.sh $label ../bb-recomp $COMMON $*" | tr -d '\r' | tee "$OUT/$label.device.txt" || rc=$?
    "${ADB[@]}" pull "$DEV/work-$label/$label.log" "$OUT/$label.log" >/dev/null 2>&1 || true
    "${ADB[@]}" pull "$DEV/work-$label/stats.txt" "$OUT/$label.stats.txt" >/dev/null 2>&1 || true
    dsh "ls -l '$DEV/work-$label/'*.so 2>/dev/null | head -n 3; rm -rf '$DEV/work-$label'" | tr -d '\r' | tee -a "$OUT/$label.device.txt" || true
    return $rc
}

first=1
cool() { (( first )) && { first=0; return; }; log "cooldown ${COOLDOWN}s"; sleep "$COOLDOWN"; }

run plan-only --plan-only || log "plan-only failed (see $OUT/plan-only.log)"
for j in $JOBS_LIST; do run "plan-only-jobs${j}" --plan-only --jobs "$j" || true; done
for j in $JOBS_LIST; do
    cool; run "every${EVERY}-jobs${j}" --every "$EVERY" --jobs "$j" -O1 --so t.so --stats stats.txt || log "run jobs=$j failed (see $OUT/every${EVERY}-jobs${j}.log)"
done

{
    echo "results in $OUT"
    for f in "$OUT"/*.log; do
        echo; echo "### $(basename "$f" .log)  (exit status $(sed -n 's/.*exit status \([0-9]*\),.*/\1/p' "$OUT/$(basename "$f" .log).device.txt" | tail -n 1), outer wall $(sed -n 's/.*outer wall time \([0-9]*\) ms.*/\1/p' "$OUT/$(basename "$f" .log).device.txt" | tail -n 1) ms)"
        grep -E "memory plan|^functions |CPU time|^wall time|compile summary|\.so: |^image |discovery" "$f" | cut -c1-300 || true
        grep -E "^-- /proc|MemTotal|MemAvailable" "$OUT/$(basename "$f" .log).device.txt" | head -n 3 || true
    done
} | tee "$OUT/summary.txt"

if [[ ${BB_CLEAN_ALL:-0} == 1 ]]; then
    log "removing $DEV from the phone"
    dsh "rm -rf '$DEV'"
else
    log "left on the phone: $DEV (binary + inputs, ~270 MB); remove with: adb shell rm -rf $DEV   (or run again with BB_CLEAN_ALL=1)"
fi
