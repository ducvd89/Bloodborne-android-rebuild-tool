// SPDX-License-Identifier: GPL-3.0-or-later
// macOS: the Linux system calls FEX makes through syscall(SYS_x, ...). Their numbers (above Darwin's range) go to bb_linux_syscall
// (linux_syscall.c), which emulates the ones the front end uses and fails the others with ENOSYS.
#pragma once
#include_next <sys/syscall.h>
#define SYS_futex 100001
#define SYS_rt_sigprocmask 100002
#define SYS_gettid 100003
#define SYS_tgkill 100004
#define SYS_statx 100005
#define SYS_renameat2 100006
#define SYS_pidfd_open 100007
#ifndef SYS_getcpu
#define SYS_getcpu 100008
#endif
#ifdef __cplusplus
extern "C"
#endif
int bb_linux_syscall(int, ...);
#define syscall(...) bb_linux_syscall(__VA_ARGS__)
