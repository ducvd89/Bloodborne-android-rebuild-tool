#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# shared/prepare and the Android app repository's tools/android/prepare are the same code (shared/prepare/SYNC.md). Compares them, ignoring the
# "SPDX-License-Identifier" header lines. usage: shared/check_prepare_sync.sh <Bloodborne-Android checkout>
# Exit: 0 the same, 1 they differ, 2 the app checkout was not found.
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
app=${1:-../Bloodborne-Android}
other=$app/tools/android/prepare
if [[ ! -d $other ]]; then echo "check_prepare_sync: $other not found (give the app repository's path)" >&2; exit 2; fi
fail=0
for f in shared/prepare/*; do
    name=${f##*/}
    [[ $name == SYNC.md ]] && continue
    if [[ ! -f $other/$name ]]; then echo "only here: $name"; fail=1; continue; fi
    if ! diff -q <(grep -v '^// SPDX-License-Identifier' "$f") <(grep -v '^// SPDX-License-Identifier' "$other/$name") >/dev/null; then echo "differs: $name"; fail=1; fi
done
for f in "$other"/*; do
    name=${f##*/}
    [[ $name == SYNC.md || -f shared/prepare/$name ]] || { echo "only in the app: $name"; fail=1; }
done
[[ $fail == 0 ]] && echo "prepare sources are in sync with the app"
exit $fail
