// SPDX-License-Identifier: GPL-2.0-or-later
// PS4 SELF/ELF parsing: scripts/prepare.py (parse_self, sfo, nid, inspect_libc) and the module reader
// of scripts/link_modules.py.
#pragma once
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "pyish.h"

struct Phdr {
    u32 type = 0, flags = 0;
    u64 offset = 0, vaddr = 0, paddr = 0, filesz = 0, memsz = 0, align = 0;
};
struct ElfHeader {
    u8 ident[16];
    u16 type, machine;
    u32 version;
    u64 entry, phoff, shoff;
    u32 flags;
    u16 ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
};
struct SelfInfo {
    Bytes elf;
    ElfHeader header;
    std::vector<Phdr> ph;
    std::vector<size_t> missing;
};

constexpr u32 PT_LOAD = 1, PT_DYNAMIC = 2, PT_TLS = 7, PT_SCE_DYNLIBDATA = 0x61000000,
              PT_SCE_PROCPARAM = 0x61000001, PT_SCE_RELRO = 0x61000010;
constexpr u64 DT_SCE_STRTAB = 0x61000035, DT_SCE_STRSIZE = 0x61000037, DT_SCE_SYMTAB = 0x61000039,
              DT_SCE_SYMTABSZ = 0x6100003f;

SelfInfo parse_self(View data);
const Phdr* find_phdr(const std::vector<Phdr>& ph, u32 type);  // next(...) minus StopIteration
const Phdr& require_phdr(const std::vector<Phdr>& ph, u32 type);  // raises StopIteration

struct SfoValue {
    bool is_int = false;
    u32 i = 0;
    std::string s;
    std::string str() const { return is_int ? std::to_string(i) : s; }  // f'{v}'
};
using Sfo = std::map<std::string, SfoValue>;
Sfo sfo(View data);
std::string nid(const std::string& name);

// The dynamic section of a module and the SCE library blob it points into.
struct Dynamic {
    std::vector<std::pair<u64, u64>> dyn;
    std::map<u64, u64> tags;
    View blob, strings;
    u64 tag(u64 id) const;  // KeyError
};
// next(type 2), the entries (stopping at the first zero tag unless stop_at_zero is false), next(type 0x61000000),
// the blob and the string table.
Dynamic open_dynamic(const SelfInfo& s, bool stop_at_zero);
std::string string_at(View strings, u64 off);
struct Sym {
    std::string name;
    u8 type, binding;
    u16 section;
    u64 value, size;
};

// Proves that this dump's _init_env is exactly RET.
bool inspect_libc_init_env_is_ret(View source);

// One of the module readers of link_modules.py.
struct Ident {
    bool valid = false;
    std::string n, lib, mod;
    int libver = 0, modver = 0;
    std::string key() const;  // equality key; invalid (None) has its own
};
struct Reloc3 {
    u64 target, kind, sym;
    i64 addend;
};
struct Module {
    SelfInfo self;
    Dynamic dyn;
    std::vector<std::pair<std::string, std::pair<std::string, int>>> libraries, modules;  // key -> (name, ver)
    std::vector<Sym> symbols;
    std::vector<Ident> identities;  // per symbol
    std::vector<Reloc3> relocs;
    // identity(name) for names with exactly three '#' parts.
    Ident identity(const std::string& name) const;
};
Module read_module(View source);
std::string encode_id(u64 value);
