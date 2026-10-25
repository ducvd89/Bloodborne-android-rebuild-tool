// SPDX-License-Identifier: GPL-3.0-or-later
// Progress reporting of a compile for the GUI, for `--machine` runs and for `--progress` (the app on the phone), and the stop request of the command line.
// compile.cpp calls these; player.cpp and main.cpp install the hooks (the other tools leave them empty).
#pragma once
#include <atomic>
#include <cstdint>
#include <string>

struct ProgressHooks {
  void (*stage)(const char* id, const char* text) = nullptr;  // a new stage begins
  void (*progress)(uint64_t done, uint64_t total) = nullptr;  // inside the current stage
  void (*status)(const char* text) = nullptr;                 // a changing detail of the stage (the speed of the PKG check); the window shows it
  void (*work)(const char* stage, uint64_t done, uint64_t total) = nullptr; // `--progress`: finished work of a stage (scan, discover, lift, compile, variants, link, json), see docs/NATIVE_CPU.md
};
extern ProgressHooks g_progress;  // bundle.cpp
inline void bbrStage(const char* id, const char* text) { if (g_progress.stage) g_progress.stage(id, text); }
inline void bbrStatus(const char* text) { if (g_progress.status) g_progress.status(text); }
inline void bbrProgress(uint64_t done, uint64_t total) { if (g_progress.progress) g_progress.progress(done, total); }
inline void bbrWork(const char* stage, uint64_t done, uint64_t total) { if (g_progress.work) g_progress.work(stage, done, total); }

// SIGTERM / SIGINT in the command line mode (main.cpp): the signal number, 0 while none came. Workers check it between functions and shards and return early.
extern std::atomic<int> g_bbrStopSignal;  // bundle.cpp
inline bool bbrStopRequested() { return g_bbrStopSignal.load(std::memory_order_relaxed) != 0; }

// "bb-recomp: <message>" on stderr, remembered as the message of the final `BBR-ERROR` line.
void bbrError(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
std::string bbrLastError();
