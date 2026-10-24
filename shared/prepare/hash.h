// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <string>

#include "pyish.h"

std::string sha256_hex(View d);
std::array<u8, 20> sha1(const u8* d, size_t n);
