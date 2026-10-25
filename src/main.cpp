// SPDX-License-Identifier: GPL-3.0-or-later
// bb-recomp: lifts the game's x86-64 code with FEX's front end. Step 1: IR census (no LLVM yet).
//   bb-recomp --elf out/eboot.elf                    census over every FDE function -> out/recomp/
//   bb-recomp --elf out/eboot.elf --dump 0x1234560   FEX IR of the function containing that vaddr
//   bb-recomp --elf out/eboot.elf --scan             just the function list
#include "census.h"
#include "discover.h"
#include "eh_frame.h"
#include "fde_scan.h"
#include "progress.h"
#include "version.h"
#ifdef BB_RECOMP_LLVM
#include "compile.h"
#include "bundle.h"
#endif
#ifdef BB_RECOMP_PLAYER
#include "aot_json.h"
#endif
#include <chrono>
#include <cerrno>
#include <cinttypes>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>
#ifndef _WIN32
#include <sys/resource.h>
#endif

static void usage() {
  fputs("usage: bb-recomp --pkgs <folder with the game PKGs> --out <dir>    (player mode: one command, see --help-player)\n"
        "       bb-recomp --elf <eboot.elf> [options]\n"
        "  --boot <file>        boot-linked image (scripts/link_modules.py, out/boot-linked.bin): the eboot plus the linked\n"
        "                       system modules (libc.prx ...) at their image offsets, one image, one unit table. --elf then only\n"
        "                       supplies the eboot's program headers\n"
        "  --prx-dir <dir>      where the .prx files of --boot are (default: <game>/sce_module next to out/)\n"
        "  --game-dir <dir>     the game folder (eboot.bin, sce_module/, sce_sys/param.sfo): with --boot it stands in for --elf and --prx-dir (eboot.bin\n"
        "                       and the .prx files are read for their program headers only), and with --so it also writes libbbaot.json (the format of\n"
        "                       player mode) next to the .so: the whole phone build is --boot, --game-dir and --so (docs/NATIVE_CPU.md, \"On the phone\")\n"
        "  --json <file>        where libbbaot.json goes (default: next to the .so); with --game-dir\n"
        "  --progress           machine-readable lines on stdout: BBR-PROGRESS <stage> <done> <total>, BBR-ERROR <message>, BBR-DONE <exit status>\n"
        "  --resume             keep every finished shard (its object and a .stat file next to the .so) and reuse those a restart finds for the same inputs\n"
        "  --clean-work         after a successful build delete the work files next to the .so (objects, .stat files, the stub and helper objects)\n"
        "                       SIGTERM / SIGINT stop the build at the next function boundary (exit status 143 / 130); the finished shards stay\n"
        "  --dump <hex vaddr>   print the FEX IR of the function containing the address\n"
        "  --scan               list function count and range statistics only\n"
        "  --funcs <file>       compare the FDE list with a 'start end' hex list (ehscan.py output)\n"
        "  --out <dir>          census output directory (default out/recomp)\n"
        "  --jobs <n>           worker threads (default: cores - 2, at most 22; fewer when the memory budget needs it)\n"
        "  --limit <n>, --every <n>   census (and compile) only the first n functions / every nth function\n"
        "  --base <hex>         host address of image vaddr 0 (default 0x2b00000000: no plain constant of the game falls in it;\n"
        "                       a multiple of 4 GB below 0x8000000000, so that it also fits a 39-bit address space; others are tried when it cannot be mapped)\n"
        "  --host <profile>     target features: armv8.2 | armv8.4 (default) | armv9\n"
        "  --no-multiblock      one basic block per unit (FEX default is multiblock; the game ships Multiblock=1)\n"
        "  --tso                lift with x86 TSO memory ordering (the game runs with it off)\n"
        "  --no-gap-units       do not lift code the entry unit does not reach as extra units\n"
        "  --no-switch-tables   do not resolve jump tables (indirect jmps go through the dispatcher)\n"
        "  --no-discovery       skip the search for code without an FDE (data pointers, branch targets outside every function)\n"
        "  --no-secondary       no extra units for C++ landing pads and setjmp return sites\n"
        "  --extra-entries <file>  addresses that must be unit entries: the runtime's misses log (aot-misses.log, `miss +0x...` lines) or one hex address per line\n"
        "  --validate           run FEX's IR validation pass on every unit\n"
        "  --no-passes          skip the x87 and dead-flag passes\n"
        "  --no-size-hist       skip ir_ops_by_size.csv\n"
        "  --inject-crash <hex>, --inject-assert <hex>   test hooks: fault / FEX assert while lifting that function\n"
        "  --no-nice            do not lower the process priority to 19\n"
        "LLVM modes (need -DBB_RECOMP_LLVM=ON):\n"
        "  --emit-ll <hex vaddr|all> <file>   LLVM IR text (one function, or --limit N / all functions)\n"
        "  --obj <file>         compile to object files (<file> for 1 shard, else <base>.NNN.o + <base>.table.o)\n"
        "  --so <file>          compile and link a shared object: Android ARM64 (NDK ld.lld) or, with --target x86_64, a PC one (clang + lld)\n"
        "  --target aarch64|x86_64   (default aarch64)     -O0 | -O1 (default) | -O2\n"
        "  --shards <n>         modules compiled in parallel (default 64 or more: the memory plan makes the shards smaller, then the\n"
        "                       worker count lower, until the peak fits in 60% of the available RAM; 1 for --emit-ll)\n"
        "  --plan-only          print the memory plan (jobs, shards, estimated peak) and stop\n"
        "  --mem-fraction <f>   share of the available memory the compile may use (default 0.6)\n"
        "  --granular off|jumps|all   granular entries: units enterable at every instruction boundary (default jumps: the functions with table-like indirect\n"
        "                       jumps the lifter could not resolve for sure; all: every function, a size and compile time experiment)\n"
        "  --granular-funcs <file>   more granular functions: the functions holding the addresses in the file (a misses log works too)\n"
        "  --dump-units <file>  write the unit table (entry, end, extra, function, granular) as text, then go on\n"
        "  --check-entries <hex,hex,...>   image offsets that must be a unit entry or a granular entry; prints each, exit status 3 when one is not\n"
        "  --units-only         stop once the unit set is known (with --dump-units / --check-entries): no code is generated\n"
        "  --verify-llvm        run the LLVM verifier on every module\n"
        "  --flags-at-calls     also store NZCV before calls and returns (x86 flags are dead there; off by default)\n"
        "  --no-dirty-check     no `if (bbr_dirty[unit]) bbr_dirty_entry()` at the start of every unit (default: on)\n"
        "  --resume-entries     every call-return block is also an entry of its unit (resume flag in the guest state)\n"
        "  --variants <file|dir>   a variant catalog (variants/*.txt; repeatable): patch groups compiled as variants, native hook sites\n"
        "  --variant-report     list every variant and hook site in the summary\n"
        "  --no-callee-saved    reload rbx, rbp, r12-r15 after calls too (default: the SysV callee-saved set is trusted)\n"
        "  --ndk <dir>          Android NDK for --so\n"
        "  --bundle-dir <dir>   self-contained build: the bundle directory (default: <exe>/../share/bb-recomp, if present)\n"
        "  --helpers-dir <dir>  the x87 helper sources (default: helpers of the build)\n"
        "  --stats <file>       also write the compile summary here\n", stderr);
}

namespace {
using clk = std::chrono::steady_clock;
bool g_prog = false;  // --progress
std::mutex g_progMu;
std::string g_progStage;
uint64_t g_progLast = ~0ull;
clk::time_point g_progAt;

// BBR-PROGRESS lines (docs/NATIVE_CPU.md): the first of a stage, the last one (done >= total) and otherwise at most four a second, only when `done` grew.
void workHook(const char* stage, uint64_t done, uint64_t total) {
  std::lock_guard<std::mutex> g(g_progMu);
  const auto now = clk::now();
  const bool fresh = g_progStage != stage;
  if (!fresh && (done <= g_progLast || (done < total && now - g_progAt < std::chrono::milliseconds(250)))) return;   // (threads report out of order: never backwards)
  g_progStage = stage; g_progLast = done; g_progAt = now;
  printf("BBR-PROGRESS %s %" PRIu64 " %" PRIu64 "\n", stage, done, total);
  fflush(stdout);
}

// What --progress prints at the end of every run: BBR-ERROR (a failure that has a message), BBR-DONE <exit status>.
void progressEnd(int rc) {
  if (rc == 0) workHook("done", 1, 1);
  else if (rc != 128 + SIGTERM && rc != 128 + SIGINT) { std::string m = bbrLastError(); if (m.empty()) m = "failed (exit status " + std::to_string(rc) + ")"; for (char& c : m) if (c == '\n' || c == '\r') c = ' '; printf("BBR-ERROR %s\n", m.c_str()); }
  printf("BBR-DONE %d\n", rc);
  fflush(stdout);
}

[[noreturn]] void usageExit() {
  usage();
  if (g_prog) { bbrError("bad command line (bb-recomp --help)"); progressEnd(2); }
  exit(2);
}

// SIGTERM / SIGINT: the workers see the flag between two functions (and shards) and return; a watchdog ends the process when something does not (a long
// optimiser or code generator run, the linker) within a few seconds. Handlers that player mode installed stay.
void onStopSignal(int sig) { g_bbrStopSignal = sig; }
void installStopHandlers() {
  bool any = false;
  for (int sig : {SIGTERM, SIGINT}) {
    auto prev = signal(sig, onStopSignal);
    if (prev != SIG_DFL) signal(sig, prev); else any = true;
  }
  if (!any) return;
  std::thread([] {
    for (;;) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      const int sig = g_bbrStopSignal.load();
      if (!sig) continue;
      std::this_thread::sleep_for(std::chrono::seconds(8));
      if (g_prog) progressEnd(128 + sig);
      fflush(stdout);
      std::_Exit(128 + sig);
    }
  }).detach();
}
int stoppedStatus() { return 128 + g_bbrStopSignal.load(); }
} // namespace

static int recompRun(int argc, char** argv);

// The command line tool. With --progress it ends every run with BBR-DONE (see progressEnd).
int recompMain(int argc, char** argv) {
  for (int i = 1; i < argc; ++i) if (!strcmp(argv[i], "--progress")) g_prog = true;
  if (!g_prog) return recompRun(argc, argv);
  setvbuf(stdout, nullptr, _IOLBF, 0);
  g_progress.work = workHook;
  const int rc = recompRun(argc, argv);
  progressEnd(rc);
  return rc;
}

static int recompRun(int argc, char** argv) {
  std::string elf = "out/eboot.elf", funcsFile, dumpArg, bootFile, prxDir, extraFile, gameDir, jsonFile;
  bool elfSet = false, baseSet = false, resume = false, cleanWork = false;
  uint64_t base = kDefaultLiftBase;  // see elf_image.h
  bool scanOnly = false, nice = true, dump = false, noSecondary = false, noDiscovery = false;
  CensusOptions co;
#ifdef BB_RECOMP_LLVM
  CompileOptions cc;
  bool shardsSet = false, jobsSet = false;
  cc.statsPath = "out/recomp/compile.txt";
  cc.bundleDir = bundleDirDefault();
#endif
  unsigned hw = std::thread::hardware_concurrency();
  co.jobs = std::min(22u, hw > 2 ? hw - 2 : 1u);
  for (int i = 1; i < argc; ++i) {
    auto is = [&](const char* s) { return !strcmp(argv[i], s); };
    auto arg = [&]() -> const char* { if (i + 1 >= argc) usageExit(); return argv[++i]; };
    if (is("--elf")) { elf = arg(); elfSet = true; }
    else if (is("--boot")) bootFile = arg();
    else if (is("--prx-dir")) prxDir = arg();
    else if (is("--game-dir")) gameDir = arg();
    else if (is("--json")) jsonFile = arg();
    else if (is("--progress")) {}   // handled by recompMain
    else if (is("--resume")) resume = true;
    else if (is("--clean-work")) cleanWork = true;
    else if (is("--dump")) { dump = true; dumpArg = arg(); }
    else if (is("--scan")) scanOnly = true;
    else if (is("--funcs")) funcsFile = arg();
    else if (is("--out")) co.outDir = arg();
    else if (is("--jobs")) { co.jobs = std::max(1, atoi(arg())); jobsSet = true; }
    else if (is("--limit")) co.limit = strtoull(arg(), nullptr, 0);
    else if (is("--every")) co.every = std::max<uint64_t>(1, strtoull(arg(), nullptr, 0));
    else if (is("--base")) { base = strtoull(arg(), nullptr, 16); baseSet = true; }
    else if (is("--host")) co.lift.host = arg();
    else if (is("--no-multiblock")) co.lift.multiblock = false;
    else if (is("--tso")) co.lift.tso = true;
    else if (is("--no-gap-units")) co.lift.gapUnits = false;
    else if (is("--no-switch-tables")) co.lift.switchTables = false;
    else if (is("--no-secondary")) noSecondary = true;
    else if (is("--no-discovery")) noDiscovery = true;
    else if (is("--extra-entries")) extraFile = arg();
    else if (is("--validate")) co.lift.validate = true;
    else if (is("--no-passes")) co.lift.passes = false;
    else if (is("--no-size-hist")) co.sizeHist = false;
    else if (is("--inject-crash")) co.lift.injectCrash = strtoll(arg(), nullptr, 16);
    else if (is("--inject-assert")) co.lift.injectAssert = strtoll(arg(), nullptr, 16);
    else if (is("--no-nice")) nice = false;
#ifdef BB_RECOMP_LLVM
    else if (is("--emit-ll")) { std::string a = arg(); cc.llPath = arg(); if (a == "all") cc.onlyVaddr = -1; else cc.onlyVaddr = strtoll(a.c_str(), nullptr, 16); }
    else if (is("--obj")) cc.objPath = arg();
    else if (is("--so")) cc.soPath = arg();
    else if (is("--target")) { std::string t = arg(); if (t == "aarch64") cc.target.aarch64 = true; else if (t == "x86_64") cc.target.aarch64 = false; else usageExit(); }
    else if (is("-O0")) cc.target.opt = 0;
    else if (is("-O1")) cc.target.opt = 1;
    else if (is("-O2")) cc.target.opt = 2;
    else if (is("--shards")) { cc.shards = std::max(1, atoi(arg())); shardsSet = true; }
    else if (is("--granular")) { std::string g = arg(); if (g == "off") cc.granular = CompileOptions::Granular::Off; else if (g == "jumps") cc.granular = CompileOptions::Granular::Jumps; else if (g == "all") cc.granular = CompileOptions::Granular::All; else usageExit(); }
    else if (is("--granular-funcs")) cc.granularList = arg();
    else if (is("--dump-units")) cc.unitsDump = arg();
    else if (is("--check-entries")) {
      std::string l = arg();
      for (size_t p = 0; p < l.size();) {
        char* e = nullptr; const char* c = l.c_str() + p;
        if (*c == '+') ++c;
        const uint64_t a = strtoull(c, &e, 16);
        if (e == c) break;
        cc.checkEntries.push_back(a);
        p = e - l.c_str(); while (p < l.size() && (l[p] == ',' || l[p] == ' ')) ++p;
      }
    }
    else if (is("--units-only")) cc.unitsOnly = true;
    else if (is("--verify-llvm")) cc.verify = true;
    else if (is("--plan-only")) cc.planOnly = true;
    else if (is("--mem-fraction")) cc.memFraction = std::max(0.05, atof(arg()));
    else if (is("--flags-at-calls")) cc.storeFlagsAtCalls = true;
    else if (is("--resume-entries")) cc.resumeEntries = true;
    else if (is("--no-dirty-check")) cc.dirtyCheck = false;
    else if (is("--no-callee-saved")) cc.calleeSaved = false;
    else if (is("--variants")) cc.variantPaths.push_back(arg());
    else if (is("--variant-report")) cc.variantReport = true;
    else if (is("--ndk")) cc.ndk = arg();
    else if (is("--bundle-dir")) cc.bundleDir = arg();
    else if (is("--helpers-dir")) cc.helpersDir = arg();
    else if (is("--stats")) cc.statsPath = arg();
    else if (is("--func")) cc.onlyVaddr = strtoll(arg(), nullptr, 16);
#endif
    else usageExit();
  }
  co.jobs = std::min(co.jobs, 22u); // 2 of 24 cores stay free on the dev PC
#ifndef _WIN32
  if (nice) setpriority(PRIO_PROCESS, 0, 19);
#endif
  installStopHandlers();
  // --game-dir: the game folder stands in for --elf and --prx-dir (the .prx files and eboot.bin are SELF files: their ELF program headers are read as they are).
  if (!gameDir.empty()) {
    while (gameDir.size() > 1 && (gameDir.back() == '/' || gameDir.back() == '\\')) gameDir.pop_back();
    if (!bootFile.empty() && !elfSet) elf = gameDir + "/eboot.bin";
    if (!bootFile.empty() && prxDir.empty()) prxDir = gameDir + "/sce_module";
  }
#ifdef BB_RECOMP_LLVM
  const bool writesSo = !cc.soPath.empty();
#else
  const bool writesSo = false;
#endif
  if (!gameDir.empty() && !writesSo && !jsonFile.empty()) { bbrError("--json needs --so (the json describes the .so)"); return 2; }
  if (!gameDir.empty() && bootFile.empty() && writesSo) { bbrError("--game-dir writes libbbaot.json for a --boot image: give --boot too"); return 2; }
#ifndef BB_RECOMP_PLAYER
  if (!gameDir.empty() && writesSo) { bbrError("this build cannot write libbbaot.json (--game-dir with --so)"); return 2; }
#endif
  ElfImage img; std::string err;
  {
    // The default base is tried first; when its address range cannot be mapped (a different kernel layout), the fallbacks (see elf_image.h). A base given with --base is the only one.
    std::vector<uint64_t> bases {base};
    if (!baseSet) for (uint64_t b : kFallbackLiftBases) bases.push_back(b);
    if (!bootFile.empty() && prxDir.empty()) { bbrError("--boot needs --game-dir <game folder> or --prx-dir <game folder>/sce_module"); return 1; }
    for (size_t bi = 0; bi < bases.size(); ++bi) {
      img = ElfImage(); err.clear();
      const bool ok = bootFile.empty() ? img.load(elf, bases[bi], err) : img.loadBoot(bootFile, elf, prxDir, bases[bi], err);
      if (ok) break;
      if (!img.mapFailed || bi + 1 == bases.size()) { bbrError("%s%s", err.c_str(), bi ? " (the other lift bases were tried too)" : ""); return 1; }
      fprintf(stderr, "bb-recomp: %s; trying the lift base %#llx\n", err.c_str(), (unsigned long long)bases[bi + 1]);
    }
  }
  if (bbrStopRequested()) return stoppedStatus();
  std::vector<Func> funcs;
  bbrWork("scan", 0, 1);
  if (!scanAllFdes(img, funcs, err)) { bbrError("%s", err.c_str()); return 1; }
  uint64_t bytes = 0, mn = ~0ull, mx = 0; for (auto& f : funcs) { bytes += f.end - f.start; mn = std::min(mn, f.start); mx = std::max(mx, f.end); }
  if (img.boot) {
    printf("boot image %s: %zu modules\n", bootFile.c_str(), img.modules.size());
    for (auto& m : img.modules) {
      size_t n = 0; uint64_t b = 0;
      for (auto& f : funcs) if (f.start >= m.base && f.start < m.base + m.size) { ++n; b += f.end - f.start; }
      printf("  %-16s image +%#" PRIx64 " size %#" PRIx64 ": %zu FDE functions, %.2f MB\n", m.name.c_str(), m.base, m.size, n, b / 1e6);
    }
  }
  printf("image %s: text vaddr %#" PRIx64 "-%#" PRIx64 " mapped at base %#" PRIx64 "; %zu FDE functions, %.1f MB, range %#" PRIx64 "-%#" PRIx64 "\n",
         elf.c_str(), img.textLo, img.textHi, img.base, funcs.size(), bytes / 1e6, mn, mx);
  if (!funcsFile.empty()) {
    FILE* f = fopen(funcsFile.c_str(), "r"); if (!f) { bbrError("%s: %s", funcsFile.c_str(), strerror(errno)); return 1; }
    unsigned long long s, e; size_t n = 0, bad = 0;
    while (fscanf(f, "%llx %llx", &s, &e) == 2) { if (n >= funcs.size() || funcs[n].start != s || funcs[n].end != e) ++bad; ++n; }
    fclose(f);
    printf("compared with %s: %zu entries, %zu differ%s\n", funcsFile.c_str(), n, bad, n != funcs.size() ? " (count differs)" : "");
    if (bad || n != funcs.size()) return 1;
  }
  {
    const size_t nFde = funcs.size();
    const NonFde nf = addNonFdeEntries(img, funcs);
    if (nf.entries || nf.stubs) printf("non-FDE code the runtime enters: %zu start entries (eboot entry, module starts), %zu PLT stubs (%zu pseudo-functions added to the %zu FDE functions)\n", nf.entries, nf.stubs, funcs.size() - nFde, nFde);
  }
  static std::map<uint64_t, std::vector<uint64_t>> entryUnits;
  static EhScan eh;
  static ReturnsTwice rt;
  if (!noSecondary && !scanOnly) {
    scanLandingPads(img, funcs, eh);
    rt = findReturnsTwice(img);
    printf("secondary entries: %" PRIu64 " FDEs, %" PRIu64 " with an LSDA, %" PRIu64 " call-site records, %" PRIu64 " landing pads in %zu functions%s%s\n", eh.fdes, eh.fdesWithLsda, eh.callSites, eh.pads, eh.landingPads.size(),
           eh.badLsda ? " (unparsable: " : "", eh.badLsda ? (std::to_string(eh.badLsda) + " " + eh.firstError + ")").c_str() : "");
    if (img.boot) {
      printf("  returns-twice: %zu import(s) %s, %zu call targets (exports and PLT stubs), %zu GOT slots\n", rt.names.size(), rt.names.empty() ? "" : rt.names[0].c_str(), rt.targets.size(), rt.gotSlots.size());
    } else printf("  returns-twice: not looked for (plain eboot.elf has no import information; use --boot)\n");
    co.lift.landingPads = &eh.landingPads;
    if (!rt.targets.empty() || !rt.gotSlots.empty()) { co.lift.retTwiceTargets = &rt.targets; co.lift.retTwiceSlots = &rt.gotSlots; }
  }
  co.lift.entryUnits = &entryUnits;
  std::vector<uint64_t> extraEntries;
  if (!extraFile.empty()) {
    extraEntries = readEntryList(extraFile.c_str(), err);
    if (!err.empty()) { bbrError("%s", err.c_str()); return 1; }
    printf("extra entries: %zu addresses from %s\n", extraEntries.size(), extraFile.c_str());
    if (noDiscovery) { bbrError("--extra-entries needs the discovery (not --no-discovery)"); return 2; }
  }
  bbrWork("scan", 1, 1);
  if (bbrStopRequested()) return stoppedStatus();
  if (!noDiscovery && !scanOnly) {
    bbrWork("discover", 0, 1);
    const DiscoverResult d = discoverEntries(img, funcs, entryUnits, co.lift, co.jobs, extraEntries.empty() ? nullptr : &extraEntries);
    printf("discovery (%u rounds): %zu relocation pointers into code outside any FDE function (%zu did not decode), %zu more through rip-relative lea, %zu through branches: %zu functions added (%.2f MB), %zu call/jmp targets inside functions that no unit started at\n",
           d.rounds, d.relocCandidates, d.relocRejected, d.leaCandidates, d.branchCandidates, d.pseudoFuncs, d.pseudoBytes / 1e6, d.midEntries);
    if (!extraEntries.empty())
      printf("  extra entries: %zu addresses: %zu already function starts, %zu in the middle of functions (units), %zu new pseudo-functions, %zu rejected (outside the image or not code)\n",
             d.extraGiven, d.extraFuncStarts, d.extraMid, d.extraNew, d.extraRejected);
    if (bbrStopRequested()) return stoppedStatus();
  }
  if (scanOnly) return 0;
  if (dump) return dumpFunction(img, funcs, strtoull(dumpArg.c_str(), nullptr, 16), co.lift);
#ifdef BB_RECOMP_LLVM
  if (!cc.llPath.empty() || !cc.objPath.empty() || !cc.soPath.empty() || cc.planOnly || cc.unitsOnly || !cc.unitsDump.empty() || !cc.checkEntries.empty()) {
    cc.lift = co.lift; cc.jobs = co.jobs; cc.limit = co.limit; cc.every = co.every; cc.jobsFixed = jobsSet; cc.shardsFixed = shardsSet;
    if (!shardsSet && !cc.llPath.empty() && cc.objPath.empty() && cc.soPath.empty()) cc.shards = 1;
    cc.resume = resume;
    int rc = runCompile(img, funcs, cc);
#ifdef BB_RECOMP_PLAYER
    if (rc == 0 && writesSo && !gameDir.empty()) {
      // libbbaot.json, as player mode writes it: the game's ids from sce_sys/param.sfo, the facts of the finished .so (read back and checked against the boot image).
      bbrWork("json", 0, 1);
      GameMeta meta; SoInfo so; uint64_t imageSize = 0;
      std::string js = jsonFile;
      if (js.empty()) { js = cc.soPath; if (js.size() > 3 && js.compare(js.size() - 3, 3, ".so") == 0) js.resize(js.size() - 3); js += ".json"; }
      std::string e = readGameMeta(gameDir, meta);
      if (e.empty()) e = checkAndWriteAotJson(cc.soPath, bootFile, meta, js, so, imageSize);
      if (!e.empty()) { bbrError("%s", e.c_str()); rc = 1; }
      else {
        printf("%s: %s %s, version %s, ABI version %u, %" PRIu64 " units in %" PRIu64 " functions, image hash %s, %.2f MB\n", js.c_str(), meta.titleId.c_str(), meta.title.c_str(), meta.version.c_str(), so.abiVersion, so.unitCount, so.funcCount, hex16(so.imageHash).c_str(), so.size / 1048576.0);
        bbrWork("json", 1, 1);
      }
    }
#endif
    if (rc == 0 && cleanWork) for (const std::string& f : cc.workFiles) std::remove(f.c_str());
    return rc;
  }
#endif
  return runCensus(img, funcs, co);
}

// Player mode (player.cpp, BB_RECOMP_PLAYER): `bb-recomp --pkgs <folder> --out <dir>` (or --game <dump>) builds the runtime image and compiles it with
// recompMain(). Started without arguments (a double click), or with --gui, the window opens (gui.cpp, BB_RECOMP_GUI); --cli forces the command line.
#ifdef BB_RECOMP_PLAYER
int playerMain(int argc, char** argv);
#endif
#ifdef BB_RECOMP_GUI
#include "gui.h"
#endif
int main(int argc, char** argv) {
#ifdef BBR_TOOL_VERSION
  if (argc == 2 && (!strcmp(argv[1], "--version") || !strcmp(argv[1], "-V"))) { printf("bb-recomp %s\n", BBR_TOOL_VERSION); return 0; }
#endif
#ifdef BB_RECOMP_GUI
  if (argc == 1 || (argc >= 2 && (!strcmp(argv[1], "--gui") || !strcmp(argv[1], "--gui-selftest") || !strcmp(argv[1], "--about")))) return guiMain(argc, argv);
#endif
  if (argc >= 2 && !strcmp(argv[1], "--cli")) { --argc; ++argv; argv[0] = const_cast<char*>("bb-recomp"); }
#ifdef BB_RECOMP_PLAYER
  for (int i = 1; i < argc; ++i) if (!strcmp(argv[i], "--game") || !strcmp(argv[i], "--pkgs") || !strcmp(argv[i], "--check-pkg") || !strcmp(argv[i], "--help-player")) return playerMain(argc, argv);
#endif
  return recompMain(argc, argv);
}
