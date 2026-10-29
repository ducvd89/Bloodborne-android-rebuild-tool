// SPDX-License-Identifier: GPL-3.0-or-later
// macOS: the Linux futex operations FEX uses; bb_linux_syscall (linux_syscall.c) implements them with os_sync_wait_on_address.
#pragma once
#define FUTEX_WAIT 0
#define FUTEX_WAKE 1
#define FUTEX_LOCK_PI 6
#define FUTEX_WAIT_BITSET 9
#define FUTEX_WAKE_BITSET 10
#define FUTEX_PRIVATE_FLAG 128
#define FUTEX_CLOCK_REALTIME 256
#define FUTEX_CMD_MASK ~(FUTEX_PRIVATE_FLAG | FUTEX_CLOCK_REALTIME)
#define FUTEX_BITSET_MATCH_ANY 0xffffffff
