// SPDX-License-Identifier: GPL-3.0-or-later
// macOS: glibc's <malloc.h> for FEX (malloc_usable_size, memalign).
#pragma once
#include <stdlib.h>
#include <malloc/malloc.h>
#ifdef __cplusplus
extern "C" {
#endif
static inline size_t malloc_usable_size(void* p) { return malloc_size(p); }
static inline void* memalign(size_t align, size_t size) {
  void* p = NULL;
  if (align < sizeof(void*)) align = sizeof(void*);
  return posix_memalign(&p, align, size) == 0 ? p : NULL;
}
#ifdef __cplusplus
}
#endif
