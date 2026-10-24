#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Fetches the source of ps4-pkg-tools (https://github.com/xXJSONDeruloXx/ps4-pkg-tools, shadPS4's PKG/PFS code, GPL-2.0-or-later) at the pinned
# commit, with the submodules the PKG tool needs (cryptopp, cryptopp-cmake, fmt), each at the commit that commit pins. Shallow, no history.
#   shared/pkg-tool/fetch-ps4-pkg-tools.sh [dest]        (default: .local-deps/src/ps4-pkg-tools; BB_PKG_TOOL_DEST also names it)
# build_pkg_tool.sh calls this by itself when the source is missing. The result is unpatched: build_pkg_tool.sh applies patches/ to a copy.
# A git submodule would also fetch tracy, toml11 and zlib (about 40 MB) that the tool does not use, so this script does it instead.
set -euo pipefail
cd -- "$(dirname -- "$0")/../.."
dest=${1:-${BB_PKG_TOOL_DEST:-$PWD/.local-deps/src/ps4-pkg-tools}}
repo=${BB_PKG_TOOL_REPO:-https://github.com/xXJSONDeruloXx/ps4-pkg-tools}
commit=45baedaa29b8da42b3c6fe912800d2fa4119834b
need=(externals/cryptopp externals/cryptopp-cmake externals/fmt)

if [[ ! -f $dest/CMakeLists.txt ]]; then
    rm -rf "$dest"; mkdir -p "$dest"
    git -C "$dest" init -q
    git -C "$dest" fetch -q --depth 1 "$repo" "$commit"
    git -C "$dest" checkout -q FETCH_HEAD
fi
[[ $(git -C "$dest" rev-parse HEAD) == "$commit" ]] || { echo "$dest is not ps4-pkg-tools at $commit" >&2; exit 1; }
for p in "${need[@]}"; do
    [[ -f $dest/$p/.bb-fetched ]] && continue
    sha=$(git -C "$dest" ls-tree HEAD "$p" | awk '{print $3}')
    url=$(git config -f "$dest/.gitmodules" --get "submodule.$p.url")   # the last of a name's urls wins, as in git
    rm -rf "${dest:?}/$p"; mkdir -p "$dest/$p"
    git -C "$dest/$p" init -q
    git -C "$dest/$p" fetch -q --depth 1 "$url" "$sha"
    git -C "$dest/$p" checkout -q FETCH_HEAD
    rm -rf "$dest/$p/.git"; touch "$dest/$p/.bb-fetched"
    echo "fetched $p @ ${sha:0:10}"
done
