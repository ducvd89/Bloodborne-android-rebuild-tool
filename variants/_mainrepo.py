# SPDX-License-Identifier: GPL-3.0-or-later
"""Where the generators find the main project (https://github.com/ducvd89/Bloodborne-Android).

The catalogs in this directory are generated from sources of the main project (the patch parts, the frame rate, display and guard patchers, BBSC's
tables and hook installer, the loader's hook shim) and from the game's own files (the eboot and the linked image, which you make from your PKGs).
The generated catalogs are checked in, so building the tool needs none of this. To regenerate or check one, check the main project out and pass
its folder with --main-repo <path> or in the environment variable BB_MAIN_REPO. The generators write only into this directory.
"""
import os
import sys
from pathlib import Path


def add_argument(ap):
    ap.add_argument('--main-repo', type=Path, default=os.environ.get('BB_MAIN_REPO'),
                    help='a checkout of ducvd89/Bloodborne-Android (default: $BB_MAIN_REPO); the generators read its sources')


def require(path):
    if not path or not (Path(path) / 'tools/android/build_patch_parts.py').is_file():
        sys.exit('this generator reads the main project: check out https://github.com/ducvd89/Bloodborne-Android and pass --main-repo <path> (or set BB_MAIN_REPO)')
    return Path(path).resolve()
