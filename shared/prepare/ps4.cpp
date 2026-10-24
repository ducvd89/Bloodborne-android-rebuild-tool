// SPDX-License-Identifier: GPL-2.0-or-later
#include "ps4.h"

#include <algorithm>

#include "hash.h"

namespace {
constexpr size_t MAX_IMAGE = 512u * 1024 * 1024;

ElfHeader read_header(View d, u128 off) {
    Cur c = unpack(d, off, 64);
    ElfHeader h;
    std::memcpy(h.ident, c.p, 16);
    c.p += 16;
    h.type = c.u16_();
    h.machine = c.u16_();
    h.version = c.u32_();
    h.entry = c.u64_();
    h.phoff = c.u64_();
    h.shoff = c.u64_();
    h.flags = c.u32_();
    h.ehsize = c.u16_();
    h.phentsize = c.u16_();
    h.phnum = c.u16_();
    h.shentsize = c.u16_();
    h.shnum = c.u16_();
    h.shstrndx = c.u16_();
    return h;
}

Phdr read_phdr(View d, u128 off) {
    Cur c = unpack(d, off, 56);
    Phdr p;
    p.type = c.u32_();
    p.flags = c.u32_();
    p.offset = c.u64_();
    p.vaddr = c.u64_();
    p.paddr = c.u64_();
    p.filesz = c.u64_();
    p.memsz = c.u64_();
    p.align = c.u64_();
    return p;
}

void check_header(const ElfHeader& h) {
    if (std::memcmp(h.ident, "\x7f" "ELF\x02\x01\x01", 7) != 0 || h.machine != 62)
        raise("ValueError", "expected little-endian x86-64 ELF");
    if (h.phentsize != 56 || !(0 < h.phnum && h.phnum < 256)) raise("ValueError", "unsupported program headers");
}

SelfInfo parse_elf(View data) {
    SelfInfo r;
    r.header = read_header(data, 0);
    check_header(r.header);
    for (unsigned i = 0; i < r.header.phnum; i++) r.ph.push_back(read_phdr(data, (u128)r.header.phoff + i * 56));
    u128 end = (u128)r.header.phoff + (u128)r.header.phnum * 56;
    for (auto& p : r.ph) end = std::max(end, (u128)p.offset + p.filesz);
    if (end > MAX_IMAGE) raise("ValueError", "probe image exceeds 512 MiB limit");
    View whole = span(data, 0, end);
    r.elf.assign(whole.p, whole.p + whole.n);
    return r;
}
}  // namespace

const Phdr* find_phdr(const std::vector<Phdr>& ph, u32 type) {
    for (auto& p : ph)
        if (p.type == type) return &p;
    return nullptr;
}
const Phdr& require_phdr(const std::vector<Phdr>& ph, u32 type) {
    const Phdr* p = find_phdr(ph, type);
    if (!p) raise("StopIteration");
    return *p;
}

SelfInfo parse_self(View data) {
    if (std::memcmp(span(data, 0, 4).p, "\x7f" "ELF", 4) == 0) return parse_elf(data);
    if (std::memcmp(span(data, 0, 4).p, "O\x15=\x1d", 4) != 0)
        raise("ValueError", "expected PS4 SELF or ELF (eboot.bin is neither: an encrypted or damaged dump?)");
    u16 count = unpack(data, 24, 2).u16_();
    u64 base = 32 + (u64)count * 32;
    SelfInfo r;
    r.header = read_header(data, base);
    check_header(r.header);
    for (unsigned i = 0; i < r.header.phnum; i++)
        r.ph.push_back(read_phdr(data, (u128)base + r.header.phoff + i * 56));
    u128 end = 0;
    for (auto& p : r.ph) end = std::max(end, (u128)p.offset + p.filesz);
    if (end > MAX_IMAGE) raise("ValueError", "probe image exceeds 512 MiB limit");
    u128 header_end = (u128)r.header.phoff + (u128)r.header.phnum * 56;
    // bytearray(end); elf[:header_end] = ...: the slice assignment grows the array when needed.
    r.elf.assign((size_t)std::max(end, header_end), 0);
    View hdr = span(data, base, header_end);
    std::memcpy(r.elf.data(), hdr.p, hdr.n);
    std::vector<std::pair<u128, u128>> covered = {{0, header_end}};
    for (unsigned i = 0; i < count; i++) {
        Cur c = unpack(data, 32 + (u128)i * 32, 32);
        u64 flags = c.u64_(), off = c.u64_(), size = c.u64_(), mem = c.u64_();
        if (!(flags & 0x800)) continue;
        if (flags & 10) raise("ValueError", "encrypted/compressed SELF segment is unsupported");
        u64 index = (flags >> 20) & 4095;
        if (index >= r.ph.size()) raise("ValueError", "invalid segment index");
        const Phdr& p = r.ph[index];
        if (size != p.filesz || size != mem) raise("ValueError", "unsupported blocked segment layout");
        View src = span(data, off, size);
        std::memcpy(r.elf.data() + p.offset, src.p, src.n);
        covered.push_back({p.offset, (u128)p.offset + size});
    }
    for (size_t i = 0; i < r.ph.size(); i++) {
        const Phdr& p = r.ph[i];
        if (!p.filesz) continue;
        bool any = false;
        for (auto& [a, z] : covered)
            if (a <= p.offset && (u128)p.offset + p.filesz <= z) any = true;
        if (any) continue;
        r.missing.push_back(i);
        if (p.type == 1 || p.type == 2 || p.type == PT_SCE_DYNLIBDATA || p.type == PT_SCE_RELRO)
            raise("ValueError", "required segment " + std::to_string(i) + " is unavailable");
    }
    return r;
}

Sfo sfo(View data) {
    Cur h = unpack(data, 0, 20);
    u8 magic[4];
    std::memcpy(magic, h.p, 4);
    h.p += 4;
    h.u32_();  // version
    u32 keys = h.u32_(), values = h.u32_(), count = h.u32_();
    if (std::memcmp(magic, "\0PSF", 4) != 0) raise("ValueError", "bad SFO signature");
    Sfo result;
    for (u64 i = 0; i < count; i++) {
        Cur c = unpack(data, 20 + (u128)i * 16, 16);
        u16 key = c.u16_(), fmt = c.u16_();
        u32 size = c.u32_();
        c.u32_();  // capacity
        u32 off = c.u32_();
        u128 kstart = (u128)keys + key;
        size_t kend = index_zero(data, kstart);
        std::string k = kstart <= kend ? decode_utf8_strict({data.p + (size_t)kstart, kend - (size_t)kstart}) : std::string();
        View v = span(data, (u128)values + off, size);
        SfoValue val;
        if (fmt == 0x404) {
            if (v.n != 4) raise("struct.error", "unpack requires a buffer of 4 bytes");
            val.is_int = true;
            std::memcpy(&val.i, v.p, 4);
        } else {
            size_t n = v.n;
            while (n && v.p[n - 1] == 0) n--;
            val.s = decode_utf8_replace({v.p, n});
        }
        result[k] = val;
    }
    return result;
}

std::string nid(const std::string& name) {
    static const u8 salt[16] = {0x51, 0x8d, 0x64, 0xa6, 0x35, 0xde, 0xd8, 0xc1,
                                0xe6, 0xb0, 0x39, 0xb1, 0xc3, 0xe5, 0x52, 0x30};
    std::string in = name;
    in.append((const char*)salt, 16);
    auto d = sha1((const u8*)in.data(), in.size());
    u8 b[8];
    for (int i = 0; i < 8; i++) b[i] = d[7 - i];  // [:8][::-1]
    static const char* A = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string s;
    for (int i = 0; i < 8; i += 3) {
        u32 v = b[i] << 16 | (i + 1 < 8 ? b[i + 1] << 8 : 0) | (i + 2 < 8 ? b[i + 2] : 0);
        int chars = i + 2 < 8 ? 4 : i + 1 < 8 ? 3 : 2;
        for (int k = 0; k < chars; k++) s += A[(v >> (18 - 6 * k)) & 63];
    }
    for (auto& c : s)
        if (c == '/') c = '-';
    return s;  // no '=' padding
}

u64 Dynamic::tag(u64 id) const {
    auto it = tags.find(id);
    if (it == tags.end()) raise("KeyError", std::to_string(id));
    return it->second;
}

Dynamic open_dynamic(const SelfInfo& s, bool stop_at_zero) {
    Dynamic d;
    const Phdr& dp = require_phdr(s.ph, PT_DYNAMIC);
    View elf = view(s.elf);
    for (u128 pos = dp.offset; pos < (u128)dp.offset + dp.filesz; pos += 16) {
        Cur c = unpack(elf, pos, 16);
        u64 tag = c.u64_(), value = c.u64_();
        if (stop_at_zero && tag == 0) break;
        d.dyn.push_back({tag, value});
        d.tags[tag] = value;
    }
    const Phdr& lp = require_phdr(s.ph, PT_SCE_DYNLIBDATA);
    d.blob = span(elf, lp.offset, lp.filesz);
    u64 a = d.tag(DT_SCE_STRTAB), b = d.tag(DT_SCE_STRSIZE);
    d.strings = span(d.blob, a, b);
    return d;
}

std::string string_at(View strings, u64 off) {
    span(strings, off, 1);
    size_t end = index_zero(strings, off);
    return decode_ascii({strings.p + off, end - off});
}

bool inspect_libc_init_env_is_ret(View source) {
    SelfInfo s = parse_self(source);
    Dynamic d = open_dynamic(s, false);
    u64 a = d.tag(DT_SCE_SYMTAB), b = d.tag(DT_SCE_SYMTABSZ);
    View syms = span(d.blob, a, b);
    static const std::string init_env = nid("_init_env"), ios_init = nid("_ZNSt8ios_base4InitC1Ev");
    bool is_ret = false;
    for (u128 pos = 0; pos < syms.n; pos += 24) {
        Cur c = unpack(syms, pos, 24);
        u32 name = c.u32_();
        u8 info = c.u8_();
        c.u8_();
        u16 section = c.u16_();
        u64 value = c.u64_(), size = c.u64_();
        span(d.strings, name, 1);
        std::string symbol = decode_ascii({d.strings.p + name, index_zero(d.strings, name) - name});
        std::string key = symbol.substr(0, symbol.find('#'));
        if ((key != init_env && key != ios_init) || !section || (info & 15) != 2) continue;
        const Phdr* p = nullptr;
        for (auto& q : s.ph)
            if (q.type == 1 && (q.flags & 1) && q.vaddr <= value && value < (u128)q.vaddr + q.filesz) {
                p = &q;
                break;
            }
        if (!p) raise("StopIteration");
        View code = span(view(s.elf), (u128)p->offset + value - p->vaddr, size);
        if (key == init_env) is_ret = code.n == 1 && code.p[0] == 0xc3;
    }
    return is_ret;
}

std::string encode_id(u64 value) {
    static const char* A = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-";
    std::string r(1, A[value & 63]);
    value >>= 6;
    while (value) {
        r.insert(r.begin(), A[value & 63]);
        value >>= 6;
    }
    return r;
}

std::string Ident::key() const {
    if (!valid) return "N";
    return "V" + n + '\0' + lib + '\0' + std::to_string(libver) + '\0' + mod + '\0' + std::to_string(modver);
}

Ident Module::identity(const std::string& name) const {
    std::vector<std::string> parts;
    size_t i = 0;
    for (;;) {
        size_t j = name.find('#', i);
        parts.push_back(name.substr(i, j == std::string::npos ? j : j - i));
        if (j == std::string::npos) break;
        i = j + 1;
    }
    Ident r;
    if (parts.size() != 3) return r;  // plain names such as module_start are not linkable
    auto find = [&](const auto& table, const std::string& key) {
        for (auto& e : table)
            if (e.first == key) return e.second;
        raise("KeyError", py_repr_str(key));
    };
    auto l = find(libraries, parts[1]);  // evaluated in the tuple's order: lib, then mod
    auto m = find(modules, parts[2]);
    r.valid = true;
    r.n = parts[0];
    r.lib = l.first;
    r.libver = l.second;
    r.mod = m.first;
    r.modver = m.second;
    return r;
}

Module read_module(View source) {
    Module m;
    m.self = parse_self(source);
    m.dyn = open_dynamic(m.self, true);
    for (auto [tag, value] : m.dyn.dyn) {
        if (tag != 0x61000013 && tag != 0x61000015 && tag != 0x6100000d && tag != 0x6100000f) continue;
        auto& table = (tag == 0x61000013 || tag == 0x61000015) ? m.libraries : m.modules;
        std::string key = encode_id(value >> 48);
        std::pair<std::string, int> id = {string_at(m.dyn.strings, value & 0xffffffff), (int)((value >> 32) & 65535)};
        auto it = std::find_if(table.begin(), table.end(), [&](auto& e) { return e.first == key; });
        if (it != table.end() && it->second != id) raise("ValueError", "conflicting module/library IDs");
        if (it == table.end()) table.push_back({key, id});
        else it->second = id;
    }
    u64 a = m.dyn.tag(DT_SCE_SYMTAB), b = m.dyn.tag(DT_SCE_SYMTABSZ);
    View table = span(m.dyn.blob, a, b);
    for (u128 pos = 0; pos < table.n; pos += 24) {
        Cur c = unpack(table, pos, 24);
        u32 n = c.u32_();
        u8 info = c.u8_();
        c.u8_();
        u16 section = c.u16_();
        u64 value = c.u64_(), size = c.u64_();
        Sym s{string_at(m.dyn.strings, n), (u8)(info & 15), (u8)(info >> 4), section, value, size};
        m.identities.push_back(s.name.empty() ? Ident{} : m.identity(s.name));
        m.symbols.push_back(std::move(s));
    }
    const u64 tables[2][2] = {{0x61000029, 0x6100002d}, {0x6100002f, 0x61000031}};
    for (auto& t : tables) {
        u64 ot = m.dyn.tag(t[0]), st = m.dyn.tag(t[1]);
        View rt = span(m.dyn.blob, ot, st);
        for (u128 pos = 0; pos < rt.n; pos += 24) {
            Cur c = unpack(rt, pos, 24);
            u64 target = c.u64_(), info = c.u64_();
            i64 addend = c.i64_();
            m.relocs.push_back({target, info & 0xffffffff, info >> 32, addend});
        }
    }
    return m;
}
