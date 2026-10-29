// SPDX-License-Identifier: GPL-3.0-or-later
// macOS: Linux madvise hints FEX names. Darwin rejects unknown advice with EINVAL, which FEX ignores.
#pragma once
#include_next <sys/mman.h>
#ifndef MADV_HUGEPAGE
#define MADV_HUGEPAGE 14
#endif
#ifndef MADV_NOHUGEPAGE
#define MADV_NOHUGEPAGE 15
#endif
#ifndef MADV_DONTDUMP
#define MADV_DONTDUMP 16
#endif
// mremap: FEX's code cache (never used by bb-recomp, which does not run the JIT) names it.
#ifndef MREMAP_MAYMOVE
#define MREMAP_MAYMOVE 1
#define MREMAP_FIXED 2
#define MREMAP_DONTUNMAP 4
#include <errno.h>
static inline void* mremap(void*, size_t, size_t, int, ...) { errno = ENOSYS; return MAP_FAILED; }
#endif
