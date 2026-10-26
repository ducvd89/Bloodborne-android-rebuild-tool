// SPDX-License-Identifier: GPL-3.0-or-later
// WorkerThread: std::thread for the worker pools (lift, discover, compile, variants), with the stack size glibc gives its threads. A glibc thread
// takes RLIMIT_STACK (8 MB) as its stack; bionic's default is 1 MB, which the optimiser and FEX's front end can exceed on a deeply nested function. On
// Android (arm64/build.sh android) the thread is a pthread with an 8 MB stack; everywhere else this is plain std::thread.
#pragma once
#include <thread>
#ifdef __ANDROID__
#include <pthread.h>
#include <functional>
#include <memory>
#include <system_error>
#include <utility>

class WorkerThread {
 public:
  WorkerThread() = default;
  template <class F, class... A>
  explicit WorkerThread(F&& f, A&&... a) {
    auto fn = std::make_unique<std::function<void()>>(std::bind(std::forward<F>(f), std::forward<A>(a)...));
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 8u << 20);
    const int rc = pthread_create(&t_, &attr, [](void* p) -> void* {
      std::unique_ptr<std::function<void()>> g(static_cast<std::function<void()>*>(p));
      (*g)();
      return nullptr;
    }, fn.get());
    pthread_attr_destroy(&attr);
    if (rc != 0) throw std::system_error(rc, std::generic_category(), "pthread_create");
    fn.release();
    joinable_ = true;
  }
  WorkerThread(WorkerThread&& o) noexcept : t_(o.t_), joinable_(o.joinable_) { o.joinable_ = false; }
  WorkerThread& operator=(WorkerThread&& o) noexcept {
    if (joinable_) std::terminate();
    t_ = o.t_; joinable_ = o.joinable_; o.joinable_ = false;
    return *this;
  }
  WorkerThread(const WorkerThread&) = delete;
  WorkerThread& operator=(const WorkerThread&) = delete;
  ~WorkerThread() { if (joinable_) std::terminate(); }
  bool joinable() const { return joinable_; }
  void join() { if (joinable_) { pthread_join(t_, nullptr); joinable_ = false; } }

 private:
  pthread_t t_ {};
  bool joinable_ = false;
};
#else
using WorkerThread = std::thread;
#endif
