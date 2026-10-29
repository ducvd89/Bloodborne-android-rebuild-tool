// SPDX-License-Identifier: GPL-3.0-or-later
// macOS: included first in every FEX and bb-recomp source (-include). glibc names the code uses that Darwin spells differently.
#pragma once
#ifdef __cplusplus
#include <cerrno>
#include <cmath>
#include <cstdint>
#else
#include <errno.h>
#include <math.h>
#include <stdint.h>
#endif
#include <sys/stat.h>

#define sincos __sincos
#define st_mtim st_mtimespec
#define st_atim st_atimespec
#define st_ctim st_ctimespec

#if defined(__cplusplus) && defined(ARCHITECTURE_arm64)
// FEX's spin-wait helpers (FEXCore/Utils/SpinWaitLock.h) are overloaded for uint8/16/32/64_t; Darwin's uint64_t is unsigned long long, so the
// unsigned long (size_t) callers find no match. These declarations come before the templates that call them; a plain acquire load is a
// correct (busier) stand-in for the exclusive monitor + WFE pair.
namespace FEXCore::Utils::SpinWaitLock {
static inline unsigned long LoadExclusive(unsigned long* Futex) { return __atomic_load_n(Futex, __ATOMIC_ACQUIRE); }
static inline unsigned long WFELoadAtomic(unsigned long* Futex) { return __atomic_load_n(Futex, __ATOMIC_ACQUIRE); }
} // namespace FEXCore::Utils::SpinWaitLock
#endif
