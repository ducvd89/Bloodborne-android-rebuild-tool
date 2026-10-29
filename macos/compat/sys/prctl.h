// SPDX-License-Identifier: GPL-3.0-or-later
// macOS: Linux's prctl does not exist. FEX's front end only names it; every call fails as on an old kernel.
#pragma once
#include <errno.h>
static inline int prctl(int, ...) { errno = EINVAL; return -1; }
