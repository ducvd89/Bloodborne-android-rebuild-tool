#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Prepares the FEX source tree the build needs: FEX-Emu 2609.1 (9fbdc00bd6401aff3b32d79e78ff98b8a13e4dcf, the commit third_party/FEX pins) with the
# submodules the front end needs.
#   ./fetch-fex.sh [<FEX git checkout> [<dest dir>]]      (defaults: third_party/FEX, out/recomp-fex; CMake runs it by itself)
# - A usable checkout is one with a .git (third_party/FEX after `git submodule update --init --checkout third_party/FEX`, or any FEX clone):
#   its tracked files come from `git archive HEAD`, so the checkout itself is not modified.
# - Without one (a plain `git clone --recursive` of this repository leaves third_party/FEX empty on purpose: FEX's own submodules include test
#   binaries of about 150 MB that the tool does not use), FEX is fetched shallow at the pinned commit into .local-deps/src/FEX-git first.
# Each needed submodule is then fetched at the commit FEX pins (shallow, from the URL in FEX's .gitmodules). Needs network the first time; later
# runs are no-ops.
set -euo pipefail
cd -- "$(dirname -- "$0")"
root=$PWD
fex_url=${BB_FEX_URL:-https://github.com/FEX-Emu/FEX.git}
fex_commit=9fbdc00bd6401aff3b32d79e78ff98b8a13e4dcf
src=${1:-$root/third_party/FEX}; dest=${2:-$root/out/recomp-fex}
need=(External/fmt External/xxhash External/unordered_dense External/range-v3 Source/Common/cpp-optparse
      External/vixl External/Vulkan-Headers External/drm-headers External/jemalloc_glibc External/rpmalloc)
if [[ ! -e $src/.git ]]; then
    src=${BB_FEX_CACHE:-$root/.local-deps/src/FEX-git}
    if [[ ! -e $src/.git ]]; then
        rm -rf "$src"; mkdir -p "$src"
        git -C "$src" init -q
        git -C "$src" fetch -q --depth 1 "$fex_url" "$fex_commit"
        git -C "$src" checkout -q FETCH_HEAD
    fi
fi
src=$(realpath "$src")
[[ $(git -C "$src" rev-parse HEAD) == "$fex_commit" ]] || echo "warning: $src is not FEX at $fex_commit" >&2
mkdir -p "$dest"
if [[ ! -f $dest/.bb-fex-head ]]; then
    git -C "$src" archive HEAD | tar -x -C "$dest"
    git -C "$src" rev-parse HEAD > "$dest/.bb-fex-head"
fi
fetch() {
    local path=$1 sha url
    [[ -e $dest/$path/.bb-fetched ]] && return
    sha=$(git -C "$src" ls-tree HEAD "$path" | awk '{print $3}')
    url=$(git -C "$src" show HEAD:.gitmodules | awk -v p="$path" '$1=="path"{cur=($3==p)} cur&&$1=="url"{print $3}')
    rm -rf "${dest:?}/$path"; mkdir -p "$dest/$path"
    git -C "$dest/$path" init -q && git -C "$dest/$path" fetch -q --depth 1 "$url" "$sha" && git -C "$dest/$path" checkout -q FETCH_HEAD
    rm -rf "$dest/$path/.git"; touch "$dest/$path/.bb-fetched"
    echo "fetched $path @ ${sha:0:10}"
}
for p in "${need[@]}"; do fetch "$p" & done
wait
