// SPDX-License-Identifier: GPL-3.0-or-later
#include "aot_json.h"
#include "version.h"
#include "hash.h"
#include "io.h"
#include "ps4.h"
#include "sha256.h"

#include <llvm/Config/llvm-config.h>

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {
struct Ehdr64 { uint8_t ident[16]; uint16_t type, machine; uint32_t version; uint64_t entry, phoff, shoff; uint32_t flags; uint16_t ehsize, phentsize, phnum, shentsize, shnum, shstrndx; };
struct Phdr64 { uint32_t type, flags; uint64_t offset, vaddr, paddr, filesz, memsz, align; };
struct Shdr64 { uint32_t name, type; uint64_t flags, addr, offset, size; uint32_t link, info; uint64_t addralign, entsize; };
struct Sym64 { uint32_t name; uint8_t info, other; uint16_t shndx; uint64_t value, size; };

} // namespace

// Reads the .so; returns an empty string when it is a well-formed bb-recomp library, else what is wrong with it.
std::string readSo(const std::string& path, uint64_t imageSize, SoInfo& out) {
  std::vector<uint8_t> f;
  {
    std::ifstream in(path, std::ios::binary);
    if (!in) return "cannot read " + path;
    f.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  }
  out.size = f.size();
  if (f.size() < sizeof(Ehdr64) || memcmp(f.data(), "\x7f" "ELF", 4)) return "not an ELF file";
  Ehdr64 eh; memcpy(&eh, f.data(), sizeof eh);
  if (eh.ident[4] != 2 || eh.ident[5] != 1) return "not a 64-bit little-endian ELF";
  if (eh.machine != 183) return "not an AArch64 library (machine " + std::to_string(eh.machine) + ")";
  if (eh.type != 3) return "not a shared object";
  if (eh.shoff == 0 || eh.shentsize != sizeof(Shdr64) || eh.shoff + (uint64_t)eh.shnum * sizeof(Shdr64) > f.size()) return "no section headers";
  std::vector<Shdr64> sh(eh.shnum);
  memcpy(sh.data(), f.data() + eh.shoff, sh.size() * sizeof(Shdr64));
  std::vector<Phdr64> ph;
  for (unsigned i = 0; i < eh.phnum && eh.phoff + (uint64_t)(i + 1) * eh.phentsize <= f.size(); ++i) { Phdr64 p; memcpy(&p, f.data() + eh.phoff + (uint64_t)i * eh.phentsize, sizeof p); ph.push_back(p); }
  auto fileOff = [&](uint64_t v, uint64_t len, uint64_t& off) {
    for (auto& p : ph) if (p.type == 1 && v >= p.vaddr && v + len <= p.vaddr + p.filesz) { off = p.offset + (v - p.vaddr); return off + len <= f.size(); }
    return false;
  };
  auto str = [&](const Shdr64& strtab, uint32_t at) { return strtab.offset + at < f.size() ? std::string((const char*)f.data() + strtab.offset + at) : std::string(); };
  const Shdr64* dynsym = nullptr; const Shdr64* dynamic = nullptr;
  for (auto& s : sh) { if (s.type == 11) dynsym = &s; else if (s.type == 6) dynamic = &s; }
  if (!dynsym || dynsym->link >= sh.size() || dynsym->offset + dynsym->size > f.size()) return "no dynamic symbol table";
  const Shdr64& strtab = sh[dynsym->link];
  struct Found { uint64_t value = 0, size = 0; bool ok = false; };
  std::vector<std::pair<const char*, Found>> want = {{"bbr_units", {}}, {"bbr_unit_count", {}}, {"bbr_func_count", {}}, {"bbr_abi_flags", {}}, {"bbr_image_hash", {}}, {"bbr_abi_version", {}}, {"bbr_text_end", {}}, {"bbr_image_base", {}}, {"bbr_dirty", {}}, {"bbr_unit_end", {}}};
  for (uint64_t o = 0; o + sizeof(Sym64) <= dynsym->size; o += sizeof(Sym64)) {
    Sym64 s; memcpy(&s, f.data() + dynsym->offset + o, sizeof s);
    if (s.shndx == 0) continue;
    const std::string n = str(strtab, s.name);
    for (auto& w : want) if (n == w.first) w.second = {s.value, s.size, true};
  }
  auto get = [&](const char* name) -> const Found* { for (auto& w : want) if (!strcmp(w.first, name)) return w.second.ok ? &w.second : nullptr; return nullptr; };
  auto rd = [&](const char* name, unsigned bytes, uint64_t& v) {
    const Found* s = get(name); uint64_t off;
    if (!s || !fileOff(s->value, bytes, off)) return false;
    v = 0; memcpy(&v, f.data() + off, bytes); return true;
  };
  for (const char* req : {"bbr_units", "bbr_unit_count", "bbr_abi_version", "bbr_image_hash", "bbr_abi_flags", "bbr_func_count", "bbr_image_base"})
    if (!get(req)) return std::string("symbol ") + req + " is missing";
  uint64_t v = 0;
  if (!rd("bbr_abi_version", 4, v)) return "bbr_abi_version is not readable";
  out.abiVersion = (uint32_t)v;
  if (!rd("bbr_image_hash", 8, out.imageHash) || !rd("bbr_unit_count", 8, out.unitCount) || !rd("bbr_func_count", 8, out.funcCount) || !rd("bbr_abi_flags", 8, out.abiFlags)) return "a table header is not readable";
  rd("bbr_text_end", 8, out.textEnd);
  // The unit table: {vaddr, function pointer} pairs sorted by vaddr (the pointers are relocations: only the addresses are read).
  const Found* tab = get("bbr_units");
  uint64_t off;
  if (tab->size != out.unitCount * 16) return "bbr_units has " + std::to_string(tab->size / 16) + " entries, bbr_unit_count says " + std::to_string(out.unitCount);
  if (!out.unitCount || !fileOff(tab->value, tab->size, off)) return "the unit table is empty or not mapped";
  uint64_t prev = 0;
  for (uint64_t i = 0; i < out.unitCount; ++i) {
    uint64_t a; memcpy(&a, f.data() + off + i * 16, 8);
    if (i && a <= prev) return "the unit table is not sorted at entry " + std::to_string(i);
    if (a >= imageSize) return "unit " + std::to_string(i) + " is outside the image";
    prev = a;
  }
  if (const Found* d = get("bbr_dirty")) if (d->size != out.unitCount) return "bbr_dirty does not have one byte per unit";
  if (dynamic && dynamic->link < sh.size()) {
    const Shdr64& ds = sh[dynamic->link];
    for (uint64_t o = 0; o + 16 <= dynamic->size && dynamic->offset + o + 16 <= f.size(); o += 16) {
      int64_t tag; uint64_t val; memcpy(&tag, f.data() + dynamic->offset + o, 8); memcpy(&val, f.data() + dynamic->offset + o + 8, 8);
      if (tag == 1) out.needed.push_back(str(ds, (uint32_t)val));
      if (tag == 0) break;
    }
  }
  if (std::find(out.needed.begin(), out.needed.end(), "libbbcpu.so") == out.needed.end()) return "libbbcpu.so is not among the needed libraries";
  out.sha256 = sha256_hex(View{f.data(), f.size()});
  return "";
}


std::string isoNow() {
  std::time_t t = std::time(nullptr); std::tm g{};
#ifdef _WIN32
  gmtime_s(&g, &t);
#else
  gmtime_r(&t, &g);
#endif
  char b[32]; std::strftime(b, sizeof b, "%Y-%m-%dT%H:%M:%SZ", &g);
  return b;
}

std::string jsonStr(const std::string& s) {
  std::string r = "\"";
  for (unsigned char c : s) {
    if (c == '"') r += "\\\""; else if (c == '\\') r += "\\\\"; else if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); r += b; } else r += (char)c;
  }
  return r + "\"";
}

std::string hex16(uint64_t v) { char b[24]; snprintf(b, sizeof b, "%016" PRIx64, v); return b; }

// FNV-1a over the flat image of a boot-linked file: bbr_image_hash (compile.cpp fnv1a(img.file)). The boot file is
// "BBPROBE5", 12 header words, the module table, bindings, segments, 128 bytes per import and 32 per relocation, then the image.
bool imageHashOfBoot(const std::string& boot, uint64_t& hash, uint64_t& imageSize) {
  std::ifstream in(boot, std::ios::binary);
  if (!in) return false;
  uint64_t w[14];
  char magic[8]; in.read(magic, 8);
  if (memcmp(magic, "BBPROBE5", 8)) return false;
  in.read((char*)w, 12 * 8); // size entry ns nr ni caps procparam tls[4] mc
  const uint64_t size = w[0], ns = w[2], nr = w[3], ni = w[4], mc = w[11];
  in.seekg((std::streamoff)(8 + 12 * 8 + mc * 7 * 8));
  uint64_t nb; in.read((char*)&nb, 8);
  const uint64_t skip = nb * 24 + ns * 24 + ni * 128 + nr * 32;
  in.seekg((std::streamoff)skip, std::ios::cur);
  std::vector<char> buf(1 << 20);
  uint64_t h = 1469598103934665603ull, left = size;
  while (left && in) {
    in.read(buf.data(), (std::streamsize)std::min<uint64_t>(left, buf.size()));
    const size_t n = (size_t)in.gcount();
    if (!n) break;
    for (size_t i = 0; i < n; ++i) { h ^= (uint8_t)buf[i]; h *= 1099511628211ull; }
    left -= n;
  }
  if (left) return false;
  hash = h; imageSize = size;
  return true;
}


std::string readGameMeta(const std::string& gameDir, GameMeta& out) {
  try {
    MappedFile f = read_file((std::filesystem::path(gameDir) / "sce_sys/param.sfo").generic_string());
    Sfo info = sfo(f.view());
    auto get = [&](const char* k, std::string& dst) { auto it = info.find(k); if (it != info.end()) dst = it->second.str(); };
    get("TITLE_ID", out.titleId); get("APP_VER", out.version); get("TITLE", out.title); get("CONTENT_ID", out.contentId);
  } catch (PyError& e) {
    return "sce_sys/param.sfo cannot be read (" + e.kind + (e.msg.empty() ? "" : ": " + e.msg) + "): the game files are damaged.";
  }
  return "";
}

std::string writeAotJson(const std::string& jsonPath, const GameMeta& meta, const SoInfo& so, uint64_t imageSize, const std::string& catalogHash) {
  std::ostringstream j;
  j << "{\n"
    << "  \"format\": 1,\n"
    << "  \"kind\": \"bb-recomp-aot\",\n"
    << "  \"game_id\": " << jsonStr(meta.titleId) << ",\n"
    << "  \"game_version\": " << jsonStr(meta.version) << ",\n"
    << "  \"content_id\": " << jsonStr(meta.contentId) << ",\n"
    << "  \"game_title\": " << jsonStr(meta.title) << ",\n"
    << "  \"image_hash\": " << jsonStr(hex16(so.imageHash)) << ",\n"
    << "  \"image_size\": " << imageSize << ",\n"
    << "  \"abi_version\": " << so.abiVersion << ",\n"
    << "  \"abi_flags\": " << so.abiFlags << ",\n"
    << "  \"unit_count\": " << so.unitCount << ",\n"
    << "  \"function_count\": " << so.funcCount << ",\n"
    << "  \"tool\": " << jsonStr(std::string("bb-recomp ") + BBR_TOOL_VERSION) << ",\n"
    << "  \"compiler_version\": " << jsonStr(BBR_COMPILER_VERSION) << ",\n"
    << "  \"catalog_hash\": " << jsonStr(catalogHash.empty() ? "cbf29ce484222325" : catalogHash) << ",\n"
    << "  \"llvm\": " << jsonStr(LLVM_VERSION_STRING) << ",\n"
    << "  \"created\": " << jsonStr(isoNow()) << ",\n"
    << "  \"so_size\": " << so.size << ",\n"
    << "  \"so_sha256\": " << jsonStr(so.sha256) << "\n"
    << "}\n";
  std::ofstream o(jsonPath, std::ios::binary);
  o << j.str();
  if (!o) return "cannot write " + jsonPath;
  return "";
}

std::string checkAndWriteAotJson(const std::string& soPath, const std::string& bootPath, const GameMeta& meta, const std::string& jsonPath, SoInfo& so, uint64_t& imageSize, const std::string& catalogHash) {
  uint64_t imageHash = 0;
  if (!imageHashOfBoot(bootPath, imageHash, imageSize)) return "the game image (boot-linked.bin) is not readable.";
  const std::string bad = readSo(soPath, imageSize, so);
  if (!bad.empty()) return "the library failed its check: " + bad;
  if (so.abiVersion != kBbrAbiVersion) return "the library has ABI version " + std::to_string(so.abiVersion) + ", expected " + std::to_string(kBbrAbiVersion);
  if (so.imageHash != imageHash) return "the library's image hash " + hex16(so.imageHash) + " is not the game image's " + hex16(imageHash);
  return writeAotJson(jsonPath, meta, so, imageSize, catalogHash);
}
