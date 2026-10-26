// SPDX-License-Identifier: GPL-3.0-or-later
// Force-included (-include, bbrecomp.cmake) into FEX's JemallocDummy (FEXCore/Source/Utils/AllocatorHooks.cpp) in the Android NDK build, arm64/build.sh android.
// That file forwards FEXCore's allocator hooks to the C allocator and calls valloc(), which bionic does not have (the NDK headers do not declare it and
// libc.so does not export it). The tool never calls the hook; this is the definition glibc's valloc has (a page-aligned memalign) so that the unmodified
// FEX tree compiles.
#pragma once
#include <malloc.h>
#include <stddef.h>
#include <unistd.h>
static inline void* valloc(size_t size) { return memalign((size_t)getpagesize(), size); }
