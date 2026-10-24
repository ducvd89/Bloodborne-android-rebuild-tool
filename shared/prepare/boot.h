// SPDX-License-Identifier: GPL-2.0-or-later
// The BBPROBE2 image (scripts/prepare.py), kept in memory, and its linking into BBPROBE5
// (scripts/link_modules.py).
#pragma once
#include <array>
#include <string>
#include <vector>

#include "pyish.h"

struct Rel {
    u64 target, kind;
    i128 value;  // range-checked when written, like struct.pack('<QQqq') in the Python
    i64 addend;
};
struct Boot {
    u64 size = 0, entry = 0, flags = 0;
    std::vector<std::array<u64, 3>> segments;  // vaddr, memsz, flags
    std::vector<std::string> names;
    std::vector<Rel> relocs;
    Bytes image;
};

// link_modules.link(): reads the game's eboot.bin and modules again (as the Python does), writes
// out/boot-linked.bin and prints the "Linked modules" line.
void link_modules(const std::string& game, const std::string& out_dir, Boot& boot);
