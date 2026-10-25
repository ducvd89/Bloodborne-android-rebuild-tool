// SPDX-License-Identifier: GPL-3.0-or-later
// macOS: the system-allocator back end of FEXCore::Allocator, in place of FEX's JemallocDummy (bbrecomp.cmake). Darwin's aligned_alloc and
// posix_memalign refuse alignments below sizeof(void*), which FEX's containers ask for (fextl::FEXAlloc<uint32_t>: alignment 4).
#include <cstddef>
#include <cstdlib>
#include <malloc/malloc.h>
#include <sys/types.h>

namespace FEXCore::Allocator {
namespace {
void* alignedAlloc(size_t a, size_t s) {
  if (a < sizeof(void*)) a = sizeof(void*);
  void* p = nullptr;
  return ::posix_memalign(&p, a, s ? s : 1) == 0 ? p : nullptr;
}
} // namespace

void InitializeThread() {}
void* malloc(size_t size) { return ::malloc(size); }
void* calloc(size_t n, size_t size) { return ::calloc(n, size); }
void* memalign(size_t align, size_t s) { return alignedAlloc(align, s); }
void* valloc(size_t size) { return ::valloc(size); }
int posix_memalign(void** r, size_t a, size_t s) { return ::posix_memalign(r, a < sizeof(void*) ? sizeof(void*) : a, s); }
void* realloc(void* ptr, size_t size) { return ::realloc(ptr, size); }
void free(void* ptr) { ::free(ptr); }
size_t malloc_usable_size(void* ptr) { return ::malloc_size(ptr); }
void* aligned_alloc(size_t a, size_t s) { return alignedAlloc(a, s); }
void aligned_free(void* ptr) { ::free(ptr); }
void SetupAllocatorHooks(void* (*)(void*, size_t, int, int, int, off_t), int (*)(void*, size_t)) {}
void InitializeAllocator(size_t) {}
} // namespace FEXCore::Allocator
