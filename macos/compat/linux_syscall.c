// SPDX-License-Identifier: GPL-3.0-or-later
// macOS: the Linux system calls FEX's front end makes (see sys/syscall.h). futex: os_sync_wait_on_address (macOS 14.4); a wake wakes every
// waiter of the word (futex users re-check their condition, so the extra wake-ups are only spurious ones). rt_sigprocmask: pthread_sigmask on
// the low 32 signals.
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <time.h>
#include <os/os_sync_wait_on_address.h>
#include "sys/syscall.h"
#include "linux/futex.h"

#undef syscall

static int futex(uint32_t* addr, int op, uint32_t val, const struct timespec* timeout) {
  switch (op & FUTEX_CMD_MASK) {
  case FUTEX_WAIT:
  case FUTEX_WAIT_BITSET: {
    int r;
    if (timeout) {
      uint64_t ns = (uint64_t)timeout->tv_sec * 1000000000ull + (uint64_t)timeout->tv_nsec;
      if ((op & FUTEX_CMD_MASK) == FUTEX_WAIT_BITSET) {  // absolute CLOCK_MONOTONIC
        struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
        uint64_t n = (uint64_t)now.tv_sec * 1000000000ull + (uint64_t)now.tv_nsec;
        if (ns <= n) { errno = ETIMEDOUT; return -1; }
        ns -= n;
      }
      r = os_sync_wait_on_address_with_timeout(addr, val, 4, OS_SYNC_WAIT_ON_ADDRESS_NONE, OS_CLOCK_MACH_ABSOLUTE_TIME, ns);
    } else {
      r = os_sync_wait_on_address(addr, val, 4, OS_SYNC_WAIT_ON_ADDRESS_NONE);
    }
    if (r < 0 && errno == EFAULT) errno = EAGAIN;  // the word changed (or went away) before the wait
    return r < 0 ? -1 : 0;
  }
  case FUTEX_WAKE:
  case FUTEX_WAKE_BITSET:
    if (os_sync_wake_by_address_all(addr, 4, OS_SYNC_WAKE_BY_ADDRESS_NONE) < 0 && errno != ENOENT) return -1;
    return 0;
  default:
    errno = ENOSYS; return -1;
  }
}

int bb_linux_syscall(int nr, ...) {
  va_list ap; va_start(ap, nr);
  int r = -1;
  switch (nr) {
  case SYS_futex: {
    uint32_t* addr = va_arg(ap, uint32_t*);
    int op = va_arg(ap, int);
    uint32_t val = va_arg(ap, uint32_t);
    const struct timespec* t = va_arg(ap, const struct timespec*);
    r = futex(addr, op, val, t);
    break;
  }
  case SYS_rt_sigprocmask: {
    int how = va_arg(ap, int);
    const uint64_t* set = va_arg(ap, const uint64_t*);
    uint64_t* old = va_arg(ap, uint64_t*);
    sigset_t s = set ? (sigset_t)(*set & 0xffffffffu) : 0, o = 0;
    r = pthread_sigmask(how, set ? &s : NULL, &o) == 0 ? 0 : -1;
    if (r == 0 && old) *old = o;
    break;
  }
  case SYS_gettid: {
    uint64_t tid = 0; pthread_threadid_np(NULL, &tid);
    r = (int)tid;
    break;
  }
  case SYS_getcpu: {
    unsigned* cpu = va_arg(ap, unsigned*);
    unsigned* node = va_arg(ap, unsigned*);
    if (cpu) *cpu = 0;
    if (node) *node = 0;
    r = 0;
    break;
  }
  default:
    errno = ENOSYS;
  }
  va_end(ap);
  return r;
}
