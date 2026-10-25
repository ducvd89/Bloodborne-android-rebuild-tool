#!/system/bin/sh
# Runs on the phone (pushed and started by run-on-device.sh):  device-run-one.sh <label> <command...>
# Prints MemTotal/MemAvailable before the run, then runs the command in the background, samples temperatures, CPU frequencies and the
# process's peak RSS 5 s and 20 s into the run, waits, and prints the outer wall time and the exit status. The command's own output
# (the bb-recomp summary) goes to <label>.log in the current directory.
label=$1; shift
now_ms() { t=$(date +%s%N 2>/dev/null); case "$t" in *N|"") echo $(( $(date +%s) * 1000 ));; *) echo $(( t / 1000000 ));; esac; }
snap() {
  echo "-- sample $1 (+$(( $(now_ms) - T0 )) ms)"
  for z in /sys/class/thermal/thermal_zone*; do
    [ -r "$z/temp" ] && echo "temp $(cat "$z/type" 2>/dev/null) $(cat "$z/temp" 2>/dev/null)"
  done
  for c in /sys/devices/system/cpu/cpu[0-9]*; do
    f=$c/cpufreq/scaling_cur_freq
    [ -r "$f" ] && echo "freq ${c##*/} $(cat "$f" 2>/dev/null)"
  done
  [ -r "/proc/$pid/status" ] && grep -E 'VmHWM|VmRSS|Threads' "/proc/$pid/status"
  echo "loadavg $(cat /proc/loadavg 2>/dev/null)"
}
echo "== $label: $*"
echo "-- /proc/meminfo before the run"
grep -E 'MemTotal|MemAvailable|SwapTotal|SwapFree' /proc/meminfo
T0=$(now_ms)
pid=0
snap "before (no process)" 2>/dev/null | grep -E '^(-- |temp|freq|loadavg)'
"$@" > "$label.log" 2>&1 &
pid=$!
T0=$(now_ms)
i=0
while [ $i -lt 5 ] && kill -0 "$pid" 2>/dev/null; do sleep 1; i=$((i+1)); done
if kill -0 "$pid" 2>/dev/null; then snap "5 s"; fi
while [ $i -lt 20 ] && kill -0 "$pid" 2>/dev/null; do sleep 1; i=$((i+1)); done
if kill -0 "$pid" 2>/dev/null; then snap "20 s"; fi
wait "$pid"
rc=$?
T1=$(now_ms)
echo "-- finished: exit status $rc, outer wall time $(( T1 - T0 )) ms"
snap "after" 2>/dev/null | grep -E '^(-- |temp|freq|loadavg)'
echo "$rc" > "$label.rc"
echo "$(( T1 - T0 ))" > "$label.wall_ms"
exit 0
