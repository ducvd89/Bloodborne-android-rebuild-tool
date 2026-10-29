// SPDX-License-Identifier: GPL-3.0-or-later
// Player mode of bb-recomp: `bb-recomp --game <dump folder> [--out <dir>]`.
//
// One command, no other inputs. The player's own PS4 dump of Bloodborne (eboot.bin as a decrypted SELF, sce_module/*.prx,
// sce_sys/param.sfo) goes in; libbbaot.so and libbbaot.json come out, to be copied to the phone (the aot folder, next to eboot.bin).
// Translated game code is derived from the game, so it is made here, on the player's machine, and never distributed.
//
// Steps: (1) check the folder, (2) build the same boot image the device's bb-prepare builds (shared/prepare/*.cpp are
// compiled into this tool: bb-prepare's own main() is called, so the two cannot drift), (3) lift and compile everything with the
// memory-adaptive plan (recompMain() with the options of a developer run, plus the variant catalog when this build has one),
// (4) verify the .so, (5) write libbbaot.so and libbbaot.json.
#include "aot_json.h"
#include "bundle.h"
#include "game_check.h"
#include "hash.h"
#include "io.h"
#include "pkgs.h"
#include "pkgverify.h"
#include "sha256.h"
#include "proc.h"
#include "progress.h"
#include "ps4.h"
#include "version.h"
#include "variants.h"

#include <llvm/Config/llvm-config.h>

#include <algorithm>
#include <atomic>
#include <csignal>
#include <cctype>
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <fcntl.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#define BBR_HAVE_VARIANTS 1 // recompMain() knows --variants (variant catalogs, native hook sites)


int recompMain(int argc, char** argv);
int bb_prepare_main(int argc, char** argv); // shared/prepare/bb_prepare.cpp, main() renamed at build time

namespace fs = std::filesystem;
using clk = std::chrono::steady_clock;

namespace {
double since(clk::time_point t) { return std::chrono::duration<double>(clk::now() - t).count(); }

std::string fmtTime(double s) {
  char b[64];
  if (s < 90) snprintf(b, sizeof b, "%.0f s", s);
  else snprintf(b, sizeof b, "%d min %02d s", (int)(s / 60), (int)s % 60);
  return b;
}

bool g_machine = false;           // --machine: "@@..." lines for the GUI (STAGE id|text, PROGRESS done total, ERROR text, DONE folder)
std::atomic<bool> g_stop {false}; // SIGTERM / SIGINT: stop the PKG step; later stages end the process at once
std::atomic<bool> g_stoppable {false};
int g_step = 0, g_steps = 5;
uint64_t g_lastPct = 101;

void onStopSignal(int) { if (g_stoppable) g_stop = true; else _exit(143); }

void step(const char* id, const char* title) {
  ++g_step; g_lastPct = 101;
  if (g_machine) printf("@@STAGE %s|%s\n", id, title);
  printf("\n[%d/%d] %s\n", g_step, g_steps, title);
  fflush(stdout);
}
void hookStage(const char* id, const char* text) {
  g_lastPct = 101;
  if (g_machine) printf("@@STAGE %s|%s\n", id, text); else printf("  %s\n", text);
  fflush(stdout);
}
void hookStatus(const char* text) { if (g_machine) { printf("@@STATUS %s\n", text); fflush(stdout); } }
void hookProgress(uint64_t done, uint64_t total) {
  if (!total) return;
  if (g_machine) { printf("@@PROGRESS %" PRIu64 " %" PRIu64 "\n", done, total); fflush(stdout); return; }
  uint64_t pct = done * 100 / total;
  if (pct / 10 != g_lastPct / 10 && pct % 10 < 10) { g_lastPct = pct; printf("    %" PRIu64 " %%\n", pct / 10 * 10); fflush(stdout); }
}

[[noreturn]] void die(const std::string& msg) {
  fflush(stdout);
  if (g_machine) printf("@@ERROR %s\n", msg.c_str());
  fprintf(stderr, "\nbb-recomp: %s\n", msg.c_str());
  fflush(stdout);
  exit(1);
}

void playerUsage() {
  fputs("bb-recomp: builds everything the phone needs for the native CPU from your own copy of Bloodborne.\n"
        "\n"
        "  bb-recomp --pkgs <folder with the game's PKG files> --out <output folder>\n"
        "  bb-recomp --game <extracted game folder or its eboot.bin> [--out <dir>]     (an already extracted dump: only <dir>/aot/libbbaot.so and .json are written)\n"
        "\n"
        "  --pkgs <dir>       a folder holding Bloodborne's base game PKG and its update PKG(s) (1.09). They are unpacked, merged and\n"
        "                     compiled. The output folder then holds the game folder (CUSA03173, as the app expects it) with aot/ inside\n"
        "                     and a README.txt: copy it to the phone and choose it as the game folder in the app\n"
        "  --game <dir>       (or the path of its eboot.bin) the PS4 dump of Bloodborne with update 1.09 merged in: the folder that holds eboot.bin, sce_module/\n"
        "                     and sce_sys/ (e.g. CUSA03173). Nothing in it is changed\n"
        "  --out <dir>        the output folder (default: ./bb-recomp-out)\n"
        "  --pkg-tool <file>  ps4-pkg-tool (default: next to this program)\n"
        "  --machine          progress lines for the GUI\n"
        "  --check-pkg <file> only run the damage check on one PKG\n"
        "  --jobs <n>         worker threads (default: the CPU cores minus two, at most 22; fewer when the memory needs it)\n"
        "  --mem-fraction <f> share of the free memory the compile may use (default 0.6)\n"
        "  --variants <dir>   a variant catalog to use instead of the one next to the tool (--no-variants: none)\n"
        "  --extra-entries <file>  addresses the app reported as missing native entries: the misses log of the phone (logs/aot-misses.log) as it is.\n"
        "                     Each becomes an entry of the library; rebuild with this when the app says a native entry is missing\n"
        "  --keep-work        keep the work folder (<out>/work: the game image and the object files)\n"
        "\n"
        "It takes a few minutes and needs about 3 GB of free disk space while it works. Then copy the game folder (or just its aot folder,\n"
        "next to eboot.bin) to the phone and choose it as the game folder in the app (Menu -> Game folder): the app imports the native\n"
        "code by itself. Importing libbbaot.so and libbbaot.json by hand (Menu -> Native CPU (recompiled) -> Import) also works.\n"
        "See docs/NATIVE_CPU.md.\n", stdout);
}

// Where libbbaot.so and libbbaot.json go: an aot/ folder (the app imports it by itself from the aot folder next to eboot.bin): inside the
// game folder made from PKGs, else inside the output folder.
fs::path fromPkgsDir(bool fromPkgs, const fs::path& game, const fs::path& out) { return fromPkgs ? game / "aot" : out / "aot"; }

std::string exeDirectory() {
  std::string d = bundleExeDir();
  return d;
}

std::string readText(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  std::stringstream ss; ss << in.rdbuf();
  return ss.str();
}

// The number after `key` in the compile summary (stats file), or -1.
long long statNumber(const std::string& text, const char* key) {
  size_t at = text.find(key);
  if (at == std::string::npos) return -1;
  at += strlen(key);
  while (at < text.size() && !isdigit((unsigned char)text[at])) ++at;
  return at < text.size() ? atoll(text.c_str() + at) : -1;
}

// Runs bb-prepare's main() with stderr captured (its errors are reworded here).
int runPrepare(const std::string& game, const std::string& data, std::string& err, const fs::path& errFile) {
  std::string a0 = "bb-prepare", a1 = game, a2 = data;
  char* argv[] = {a0.data(), a1.data(), a2.data(), nullptr};
  fflush(stdout); fflush(stderr);
#ifdef _WIN32
  int saved = _dup(2); int fd = _open(errFile.string().c_str(), _O_WRONLY | _O_CREAT | _O_TRUNC, 0600); _dup2(fd, 2); _close(fd);
#else
  int saved = dup(2); int fd = open(errFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600); dup2(fd, 2); close(fd);
#endif
  int rc;
  try { rc = bb_prepare_main(3, argv); }
  catch (const std::exception& e) { fprintf(stderr, "%s\n", e.what()); rc = 1; }
  catch (...) { fprintf(stderr, "unknown error\n"); rc = 1; }
  fflush(stdout); fflush(stderr);
#ifdef _WIN32
  _dup2(saved, 2); _close(saved);
#else
  dup2(saved, 2); close(saved);
#endif
  err = readText(errFile.string());
  while (!err.empty() && (err.back() == '\n' || err.back() == '\r')) err.pop_back();
  return rc;
}
} // namespace

int playerMain(int argc, char** argv) {
  std::string gameArg, pkgsArg, outArg = "bb-recomp-out", variantsArg, jobs, memFraction, toolArg, extraArg;
  bool keepWork = false, noVariants = false;
  std::string checkOnly;
  for (int i = 1; i < argc; ++i) {
    auto is = [&](const char* s) { return !strcmp(argv[i], s); };
    auto arg = [&]() -> std::string { if (i + 1 >= argc) { playerUsage(); exit(2); } return argv[++i]; };
    if (is("--game")) gameArg = arg();
    else if (is("--pkgs")) pkgsArg = arg();
    else if (is("--out")) outArg = arg();
    else if (is("--jobs")) jobs = arg();
    else if (is("--mem-fraction")) memFraction = arg();
    else if (is("--variants")) variantsArg = arg();
    else if (is("--extra-entries")) extraArg = arg();
    else if (is("--no-variants")) noVariants = true;
    else if (is("--keep-work")) keepWork = true;
    else if (is("--pkg-tool")) toolArg = arg();
    else if (is("--machine")) g_machine = true;
    else if (is("--check-pkg")) checkOnly = arg();
    else if (is("--help-player") || is("--help") || is("-h")) { playerUsage(); return 0; }
    else { fprintf(stderr, "bb-recomp: unknown option %s (bb-recomp --help-player)\n", argv[i]); return 2; }
  }
  if (!checkOnly.empty()) {  // --check-pkg <file>: the damage check of one PKG (what the build runs first), for tests and curious players
    PkgVerifier v(checkOnly); std::string e; PkgReport r;
    if (!v.open(e)) { fprintf(stderr, "%s\n", e.c_str()); return 1; }
    PkgInfo info;
    if (!readPkgHeader(checkOnly, info, e)) { printf("%s: %s\n", checkOnly.c_str(), e.c_str()); return 1; }
    bool ok = v.run(r, e, nullptr, nullptr);
    if (!ok) { printf("%s: %s\n", checkOnly.c_str(), e.c_str()); return 1; }
    printf("%s: %s %s v%s, %.2f GB checked (%s), %s\n", checkOnly.c_str(), info.titleId.c_str(), info.category.c_str(), info.appVer.c_str(), r.bytesRead / 1073741824.0,
           Sha256::hardware() ? "SHA extensions" : "portable SHA", r.ok() ? "no damage found" : "DAMAGED");
    if (!r.ok()) printf("%s\n", r.describe().c_str());
    for (auto& m : r.matched) printf("  matched: %s\n", m.c_str());
    return r.ok() ? 0 : 1;
  }
  if (gameArg.empty() == pkgsArg.empty()) { playerUsage(); return 2; }
  const bool fromPkgs = !pkgsArg.empty();
  g_steps = fromPkgs ? 7 : 5;
  signal(SIGINT, onStopSignal); signal(SIGTERM, onStopSignal);
  g_progress.stage = hookStage; g_progress.progress = hookProgress; g_progress.status = hookStatus;
  setvbuf(stdout, nullptr, _IOLBF, 0);

  const auto t0 = clk::now();
  printf("bb-recomp %s (LLVM %s): builds what the phone needs for the native CPU, from your own copy of Bloodborne.\n", BBR_TOOL_VERSION, LLVM_VERSION_STRING);
  printf("The result is derived from your game: keep it for yourself and never share it.\n");

  std::error_code ec;
  fs::path out = fs::absolute(outArg, ec);
  fs::create_directories(out, ec);
  if (!fs::is_directory(out)) die("cannot create the output folder " + out.string());
  fs::path work = out / ".bbwork";
  fs::remove_all(work, ec);
  fs::create_directories(work, ec);
  if (!fs::is_directory(work)) die("cannot create the work folder " + work.string());
  auto cleanup = [&] { if (!keepWork) { std::error_code e2; fs::current_path(out, e2); fs::remove_all(work, e2); } };
  std::string err;

  fs::path game;
  if (fromPkgs) {
    // ---- the PKG files: find, identify, unpack, merge ----
    step("check", "Finding the PKG files");
    PkgSet set;
    if (!findPkgs(fs::absolute(pkgsArg, ec).string(), set, err)) { cleanup(); die(err); }
    printf("  base game: %s (%s, version %s, %.1f GB)\n", set.base.file.c_str(), set.titleId.c_str(), set.base.appVer.c_str(), set.base.size / 1073741824.0);
    for (auto& u : set.updates) printf("  update:    %s (version %s, %.1f GB)\n", u.file.c_str(), u.appVer.c_str(), u.size / 1073741824.0);
    step("pkgcheck", "Checking the PKGs for damage (reading every byte: a few minutes)");
    g_stoppable = true;
    const bool pkgsOk = checkPkgs(set, err, [](const std::string& l) { printf("  %s\n", l.c_str()); fflush(stdout); }, [] { return g_stop.load(); });
    g_stoppable = false;
    if (!pkgsOk) { cleanup(); die(err); }
    step("extract", "Unpacking the game (this takes a while: the game is tens of GB)");
    std::string tool = toolArg;
    if (tool.empty()) {
      const std::string b = bundleDirDefault();
      for (const std::string& c : {exeDirectory() + "/ps4-pkg-tool", exeDirectory() + "/ps4-pkg-tool.exe", b.empty() ? std::string() : b + "/../../bin/ps4-pkg-tool"})
        if (!c.empty() && fs::is_regular_file(c, ec)) { tool = c; break; }
    }
    if (const char* e = getenv("BB_PKG_TOOL")) if (toolArg.empty() && *e) tool = e;
    g_stoppable = true;
    std::string gameDir;
    const bool ok = installPkgs(set, tool, out.string(), work.string(), gameDir, err, [](const std::string& l) { printf("  %s\n", l.c_str()); fflush(stdout); }, [] { return g_stop.load(); });
    g_stoppable = false;
    if (!ok) { cleanup(); die(err); }
    game = gameDir;
  } else {
    step("check", "Checking the game folder");
    game = fs::absolute(gameArg, ec);
    if (fs::is_regular_file(game, ec)) {  // the eboot.bin itself: its folder is the game
      std::string n = game.filename().string(); for (auto& c : n) c = (char)tolower((unsigned char)c);
      if (n == "eboot.bin") game = game.parent_path();
    }
    if (!fs::is_directory(game)) { cleanup(); die("the game folder " + gameArg + " does not exist."); }
  }
  const char* needed[] = {"eboot.bin", "sce_module/libc.prx", "sce_module/libSceFios2.prx", "sce_sys/param.sfo"};
  std::string missing;
  for (const char* n : needed) if (!fs::is_regular_file(game / n)) missing += std::string("  ") + n + "\n";
  if (!missing.empty()) {
    std::string hint;
    for (auto& e : fs::directory_iterator(game, ec))
      if (e.is_directory() && fs::is_regular_file(e.path() / "eboot.bin")) hint += "  " + e.path().string() + "\n";
    cleanup();
    die("this does not look like a Bloodborne game folder. Missing in " + game.string() + ":\n" + missing +
        (hint.empty() ? std::string("Pass the folder that contains eboot.bin, sce_module and sce_sys (for example CUSA03173), with the 1.09 update merged in.")
                      : "Did you mean one of these?\n" + hint));
  }
  GameMeta meta;
  { const std::string e = readGameMeta(game.generic_string(), meta); if (!e.empty()) { cleanup(); die(e); } }
  const std::string &titleId = meta.titleId, &version = meta.version, &title = meta.title;
  printf("  %s (%s), version %s, %s\n", title.c_str(), titleId.c_str(), version.c_str(), game.generic_string().c_str());

  // ---- the game image: bb-prepare's code ----
  step("prepare", "Building the game image (the same step the app runs on the phone)");
  auto tp = clk::now();
  fs::path data = work / "data";
  if (runPrepare(game.generic_string(), data.generic_string(), err, work / "prepare.err") != 0) {
    cleanup();
    std::string text = err;
    const std::string kCheck = "prepare.GameCheckError: ";
    if (text.rfind(kCheck, 0) == 0)
      die("this game is not supported.\n  " + text.substr(kCheck.size()) + "\nbb-recomp compiles Bloodborne with the 1.09 update merged in (the same check the app makes).");
    die("the game files could not be read:\n  " + text + "\nThe PKGs or the dump may be damaged: get them again.");
  }
  const fs::path boot = data / "boot-linked.bin";
  uint64_t imageHash = 0, imageSize = 0;
  if (!imageHashOfBoot(boot.string(), imageHash, imageSize)) { cleanup(); die("the game image (boot-linked.bin) is not readable."); }
  // The eboot's own ELF (what scripts/prepare.py writes as eboot.elf): the lifter reads its program headers from it.
  fs::path elf = work / "eboot.elf";
  try {
    MappedFile f = read_file((game / "eboot.bin").generic_string());
    SelfInfo s = parse_self(f.view());
    std::ofstream o(elf, std::ios::binary); o.write((const char*)s.elf.data(), (std::streamsize)s.elf.size());
    if (!o) throw PyError{"OSError", "cannot write " + elf.string()};
  } catch (PyError& e) { cleanup(); die("eboot.bin cannot be read: " + e.kind + ": " + e.msg); }
  printf("  image ready: %.1f MB, hash %s, %s\n", imageSize / 1048576.0, hex16(imageHash).c_str(), fmtTime(since(tp)).c_str());

  // ---- lift and compile ----
  step("analyse", "Analysing and recompiling the game's code (a few minutes; the memory plan below fits your RAM)");
  const auto tc = clk::now();
  std::vector<std::string> args = {"bb-recomp", "--boot", boot.string(), "--elf", elf.string(), "--prx-dir", (game / "sce_module").string(),
                                   "--so", "libbbaot.so", "--stats", (work / "compile.txt").string(), "-O1"};
  if (!jobs.empty()) { args.push_back("--jobs"); args.push_back(jobs); }
  if (!memFraction.empty()) { args.push_back("--mem-fraction"); args.push_back(memFraction); }
  if (!extraArg.empty()) {  // the compile runs inside the work folder: an absolute path, checked now
    std::error_code xec;
    const fs::path extra = fs::absolute(extraArg, xec);
    if (xec || !fs::is_regular_file(extra, xec)) { cleanup(); die("--extra-entries: cannot read " + extraArg); }
    args.push_back("--extra-entries"); args.push_back(extra.string());
  }
  std::string variantNote = "none (this build has no variant support)", catalogHashHex;
#ifdef BBR_HAVE_VARIANTS
  {
    std::string vdir = variantsArg;
    if (noVariants) variantNote = "off (--no-variants)";
    else {
      if (vdir.empty()) {
        // next to the tool (the package: <exe>/../share/bb-recomp/variants), or the source tree's variants
        const std::string b = bundleDirDefault();
        if (!b.empty() && fs::is_directory(b + "/variants")) vdir = b + "/variants";
        else if (fs::is_directory(std::string(BBR_HELPERS_DIR) + "/../variants")) vdir = std::string(BBR_HELPERS_DIR) + "/../variants";
      }
      if (!vdir.empty() && fs::is_directory(vdir)) { args.push_back("--variants"); args.push_back(fs::absolute(vdir).string()); variantNote = vdir; }
      else variantNote = "none found";
      { uint64_t ch = 0; std::string ce; catalogHash(vdir.empty() ? std::vector<std::string>() : std::vector<std::string>{vdir}, ch, ce); char hx[20]; snprintf(hx, sizeof hx, "%016" PRIx64, ch); catalogHashHex = hx; }
    }
  }
#else
  if (!variantsArg.empty() || noVariants) printf("  note: this build has no variant catalog support; --variants/--no-variants ignored\n");
  (void)variantsArg; (void)noVariants;
#endif
  printf("  variant catalog: %s\n", variantNote.c_str());
  fflush(stdout);
  // The compile runs with the work folder as the working directory and relative names for its objects and the .so (its
  // external commands are plain strings: no spaces in them).
  fs::current_path(work, ec);
  std::vector<char*> av;
  for (auto& a : args) av.push_back(a.data());
  av.push_back(nullptr);
  const int rc = recompMain((int)args.size(), av.data());
  fflush(stdout);
  if (rc != 0) { cleanup(); die("the compile failed (code " + std::to_string(rc) + "); see the messages above."); }
  const double compileSecs = since(tc);

  // ---- verify ----
  step("verify", "Checking the result");
  SoInfo so;
  const std::string bad = readSo((work / "libbbaot.so").string(), imageSize, so);
  if (!bad.empty()) { cleanup(); die("the library failed its check: " + bad); }
  if (so.abiVersion != kBbrAbiVersion) { cleanup(); die("the library has ABI version " + std::to_string(so.abiVersion) + ", expected " + std::to_string(kBbrAbiVersion)); }
  if (so.imageHash != imageHash) { cleanup(); die("the library's image hash " + hex16(so.imageHash) + " is not the game image's " + hex16(imageHash)); }
  const std::string stats = readText((work / "compile.txt").string());
  const long long liftFail = statNumber(stats, "lift failures"), trapUnits = statNumber(stats, "units with bbr_trap (unsupported ops):");
  printf("  ELF AArch64 shared object, %.1f MB, needs %s\n", so.size / 1048576.0, [&] { std::string s; for (auto& n : so.needed) s += (s.empty() ? "" : ", ") + n; return s; }().c_str());
  printf("  ABI version %u, flags 0x%" PRIx64 ", %" PRIu64 " units in %" PRIu64 " functions, sorted and inside the image\n", so.abiVersion, so.abiFlags, so.unitCount, so.funcCount);
  printf("  image hash %s matches the game image\n", hex16(so.imageHash).c_str());
  if (liftFail > 0) printf("  note: %lld functions could not be lifted; the runtime runs them on its own CPU emulation\n", liftFail);
  if (trapUnits > 0) printf("  note: %lld units use instructions this tool cannot translate yet; the runtime runs those on its own CPU emulation\n", trapUnits);

  // ---- write ----
  step("finish", "Writing the result");
  const fs::path aotDir = fromPkgsDir(fromPkgs, game, out);
  fs::create_directories(aotDir, ec);
  const fs::path soOut = aotDir / "libbbaot.so", jsonOut = aotDir / "libbbaot.json";
  fs::remove(soOut, ec);
  fs::rename(work / "libbbaot.so", soOut, ec);
  if (ec) { ec.clear(); fs::copy_file(work / "libbbaot.so", soOut, fs::copy_options::overwrite_existing, ec); }
  if (ec) { cleanup(); die("cannot write " + soOut.string() + ": " + ec.message()); }
  { const std::string e = writeAotJson(jsonOut.string(), meta, so, imageSize, catalogHashHex); if (!e.empty()) { cleanup(); die(e); } }
  {
    std::ofstream o(out / "bb-recomp.log", std::ios::binary);
    o << "bb-recomp " << BBR_TOOL_VERSION << " (LLVM " << LLVM_VERSION_STRING << "), " << isoNow() << "\n" << stats;
  }
  if (fromPkgs) {
    std::ofstream o(out / "README.txt", std::ios::binary);
    o << "Bloodborne for Android: the game folder, ready for the app\n"
         "==========================================================\n\n"
         "What is in here\n"
         "  " << game.filename().string() << "/    Bloodborne " << version << " (" << title << "), unpacked from your PKG files, in the layout the app expects\n"
         "  " << game.filename().string() << "/aot/libbbaot.so and libbbaot.json   the game's code compiled for your phone (the native CPU)\n\n"
         "What to do\n"
         "  1. Copy the folder \"" << game.filename().string() << "\" to your phone's storage (internal storage or an SD card), for example into Download.\n"
         "     (Copying this whole folder, with this README, works too: the app finds the game folder inside.)\n"
         "  2. In the Bloodborne app, choose that folder as the game folder (Menu > Game folder).\n"
         "  3. The app copies aot/libbbaot.so into its own files and imports it by itself when the game starts (Menu > Native CPU shows the state).\n"
         "     It only copies again when the files change. Restart the game if it was already running.\n"
         "     (Game already installed on the phone? Copy only the \"aot\" folder, next to eboot.bin in the game folder there.)\n"
         "  Importing aot/libbbaot.so and aot/libbbaot.json by hand (Menu > Native CPU > Import) also works.\n\n"
         "These files are made from your own game and are for you only: do not share or upload them.\n"
         "Made by bb-recomp " << BBR_TOOL_VERSION << " on " << isoNow() << ".\n";
  }
  cleanup();
  printf("\nDone in %s (compile %s).\n\n", fmtTime(since(t0)).c_str(), fmtTime(compileSecs).c_str());
  if (fromPkgs) {
    printf("  %s\n  %s  (%.1f MB)\n\n", game.generic_string().c_str(), soOut.generic_string().c_str(), so.size / 1048576.0);
    printf("Copy the folder \"%s\" to your phone's storage and choose it as the game folder in the app (Menu > Game folder).\n"
           "The app imports the native code (aot/libbbaot.so) by itself; restart the game if it was already running.\n"
           "(Game already installed on the phone? Copy only the aot folder, next to eboot.bin.)\n"
           "These files are derived from your game: do not share them.\n", game.filename().string().c_str());
    if (g_machine) printf("@@DONE %s\n", game.generic_string().c_str());
  } else {
    printf("  %s  (%.1f MB)\n  %s\n\n", soOut.generic_string().c_str(), so.size / 1048576.0, jsonOut.generic_string().c_str());
    printf("Copy the aot folder next to eboot.bin in your game folder on the phone; the app imports it by itself when you choose the game folder\n"
           "(Menu > Game folder). Restart the game if it was already running.\n"
           "Importing both files by hand (Menu > Native CPU (recompiled) > Import) also works. These files are derived from your game: do not share them.\n");
    if (g_machine) printf("@@DONE %s\n", out.generic_string().c_str());
  }
  return 0;
}
