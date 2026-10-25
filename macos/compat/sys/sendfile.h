// SPDX-License-Identifier: GPL-3.0-or-later
// macOS: Linux's sendfile(out_fd, in_fd, offset, count) as a read/write loop.
#pragma once
#include <sys/types.h>
#include <unistd.h>
static inline ssize_t bb_linux_sendfile(int out, int in, off_t* off, size_t count) {
  char buf[65536]; size_t done = 0;
  while (done < count) {
    size_t want = count - done < sizeof buf ? count - done : sizeof buf;
    ssize_t n = off ? pread(in, buf, want, *off + (off_t)done) : read(in, buf, want);
    if (n <= 0) return done ? (ssize_t)done : n;
    for (ssize_t w = 0; w < n;) { ssize_t k = write(out, buf + w, (size_t)(n - w)); if (k < 0) return -1; w += k; }
    done += (size_t)n;
  }
  if (off) *off += (off_t)done;
  return (ssize_t)done;
}
#define sendfile bb_linux_sendfile
