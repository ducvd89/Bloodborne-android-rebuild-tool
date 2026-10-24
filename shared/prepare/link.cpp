// SPDX-License-Identifier: GPL-2.0-or-later
#include <algorithm>
#include <cstdio>
#include <unordered_map>

#include "boot.h"
#include "io.h"
#include "ps4.h"

namespace {
constexpr u64 LIMIT = 512u * 1024 * 1024;
const u8 FS_LOAD[9] = {0x64, 0x48, 0x8b, 0x04, 0x25, 0, 0, 0, 0};  // mov rax, fs:[0]

// Rewrite initial-exec `mov rax, fs:[0]` to GS: glibc owned FS on Linux (kept for the same bytes).
u64 patch_fs_loads(Bytes& image, const std::vector<Phdr>& ph, u64 base) {
    u64 patched = 0;
    for (auto& p : ph) {
        if (p.type != 1 || !(p.flags & 1)) continue;
        u128 start = (u128)base + p.vaddr, end = (u128)base + p.vaddr + p.filesz;
        if (end > image.size()) end = image.size();
        size_t at = (size_t)start;
        while (start < end && at < end) {
            const void* f = memmem(image.data() + at, (size_t)end - at, FS_LOAD, sizeof FS_LOAD);
            if (!f) break;
            size_t pos = (const u8*)f - image.data();
            image[pos] = 0x65;
            patched++;
            at = pos + sizeof FS_LOAD;
        }
    }
    return patched;
}

struct Export {
    u64 address;
    u64 kind;
};
struct TableEntry {
    u64 base, size, init, tls_address, tls_memsz, tls_filesz, tls_module;
    std::string file;
};
struct Binding {
    u64 index, address, kind;
};

void put_q(Bytes& image, u64 target, u64 v) { std::memcpy(image.data() + target, &v, 8); }
}  // namespace

void link_modules(const std::string& game, const std::string& out_dir, Boot& boot) {
    static const char* const DEFAULT_MODULES[] = {"libc.prx", "libSceFios2.prx"};
    Module main;
    {
        MappedFile f = read_file(path_join(game, "eboot.bin"));
        main = read_module(f.view());
    }
    main.self.elf = Bytes();  // only the headers, symbols and tables are needed from here on

    std::vector<std::array<u64, 3>> segments = boot.segments;
    std::vector<Rel> relocs = std::move(boot.relocs);
    std::vector<std::string> names = std::move(boot.names);
    Bytes image = std::move(boot.image);
    u64 size = boot.size, entry = boot.entry, flags = boot.flags;

    // identities = {main['identity'](name): i ...}: a dict (first position kept, last index wins).
    std::vector<Ident> id_list;
    std::vector<u64> id_index;
    std::unordered_map<std::string, size_t> id_pos;
    auto set_identity = [&](const Ident& id, u64 index) {
        auto [it, fresh] = id_pos.emplace(id.key(), id_list.size());
        if (fresh) {
            id_list.push_back(id);
            id_index.push_back(index);
        } else id_index[it->second] = index;
    };
    for (size_t i = 0; i < names.size(); i++) set_identity(main.identity(names[i]), i);
    // main_libraries = {identity: key}: the last key of an identity wins.
    std::unordered_map<std::string, std::string> main_libraries, main_modules;
    auto idkey = [](const std::pair<std::string, int>& id) { return id.first + '\0' + std::to_string(id.second); };
    for (auto& [k, id] : main.libraries) main_libraries[idkey(id)] = k;
    for (auto& [k, id] : main.modules) main_modules[idkey(id)] = k;

    auto imported = [&](const Ident& key) -> u64 {
        auto it = id_pos.find(key.key());
        if (it == id_pos.end()) {
            if (!key.valid) raise("TypeError", "cannot unpack non-iterable NoneType object");
            auto l = main_libraries.find(idkey({key.lib, key.libver}));
            auto m = main_modules.find(idkey({key.mod, key.modver}));
            std::string name = (l != main_libraries.end() && m != main_modules.end())
                                   ? key.n + '#' + l->second + '#' + m->second
                                   : key.n + '#' + key.lib;
            set_identity(key, names.size());
            names.push_back(name);
            return names.size() - 1;
        }
        return id_index[it->second];
    };

    std::unordered_map<std::string, Export> exports;  // identity key -> export
    std::unordered_map<std::string, std::vector<Export>> by_nid;  // (nid, library name)
    std::vector<TableEntry> table;
    u64 base = (size + 65535) & ~(u64)65535;
    u64 fs_patched = patch_fs_loads(image, main.self.ph, 0);
    u64 tls_module = 2;
    for (const char* filename : DEFAULT_MODULES) {
        std::string fname = filename;
        MappedFile f = read_file(path_join(path_join(game, "sce_module"), fname));
        Module m = read_module(f.view());
        f.close();
        std::vector<Phdr> loads;
        for (auto& p : m.self.ph)
            if (p.type == 1 || p.type == PT_SCE_RELRO) loads.push_back(p);
        if (loads.empty()) raise("ValueError", "max() iterable argument is empty");
        u128 modsize = 0;
        for (auto& p : loads) modsize = std::max(modsize, (u128)p.vaddr + p.memsz);
        if ((u128)base + modsize > LIMIT) raise("ValueError", "linked image exceeds probe limit");
        image.resize((size_t)(base + modsize), 0);
        auto mapped = [&](u128 address, u128 length = 8) {
            for (auto& p : loads)
                if (p.vaddr <= address && address + length <= (u128)p.vaddr + p.memsz) return true;
            return false;
        };
        for (auto& p : loads) {
            if (p.filesz > p.memsz) raise("ValueError", fname + ": invalid segment size");
            View src = span(view(m.self.elf), p.offset, p.filesz);
            std::memcpy(image.data() + base + p.vaddr, src.p, src.n);
            segments.push_back({base + p.vaddr, p.memsz, p.flags});
        }
        const Phdr* tls = find_phdr(m.self.ph, PT_TLS);
        u64 module_id = 0;
        if (tls) {
            if (tls->filesz > tls->memsz || !mapped(tls->vaddr, tls->memsz))
                raise("ValueError", fname + ": unsupported TLS layout");
            module_id = tls_module++;
        }
        for (auto& r : m.relocs) {
            if (!mapped(r.target)) raise("ValueError", fname + ": relocation outside image " + hex(r.target));
            u64 target = r.target + base;
            if (r.kind == 8) {
                relocs.push_back({target, 0, (i128)base + r.addend, 0});
            } else if (r.kind == 16) {
                if (r.sym >= m.symbols.size()) raise("IndexError", "list index out of range");
                const Sym& s = m.symbols[r.sym];
                if (!s.name.empty() || s.type != 3 || r.addend || !module_id)
                    raise("ValueError", fname + ": unsupported external TLS relocation");
                put_q(image, target, module_id);
            } else if (r.kind == 1 || r.kind == 6 || r.kind == 7) {
                if (r.sym >= m.symbols.size()) raise("IndexError", "list index out of range");
                const Sym& s = m.symbols[r.sym];
                if (s.section) {
                    if (s.section == 0xfff1) put_q(image, target, checked_q_unsigned((i128)s.value + r.addend));
                    else relocs.push_back({target, 0, (i128)base + s.value + r.addend, 0});
                } else if (!m.identities[r.sym].valid && s.binding == 2) {
                    put_q(image, target, 0);  // weak undefined (module_start/stop)
                } else {
                    u64 rk = s.type == 1 ? 2 : 1;
                    if ((rk == 1 && r.addend) || !(0 <= r.addend && r.addend < 4096))
                        raise("ValueError", fname + ": unsupported import addend");
                    relocs.push_back({target, rk, (i64)imported(m.identities[r.sym]), r.addend});
                }
            } else {
                raise("ValueError", fname + ": unsupported relocation type " + std::to_string(r.kind));
            }
        }
        u64 count = 0;
        for (size_t si = 0; si < m.symbols.size(); si++) {
            const Sym& s = m.symbols[si];
            const Ident& id = m.identities[si];
            if (s.section && (s.binding == 1 || s.binding == 2) && id.valid && (s.type == 1 || s.type == 2)) {
                if (!mapped(s.value, std::max<u64>(1, s.size))) raise("ValueError", fname + ": export outside load segments");
                if (exports.count(id.key())) raise("ValueError", fname + ": ambiguous export");
                Export e{base + s.value, s.type == 1 ? 2u : 1u};
                exports[id.key()] = e;
                by_nid[id.n + '\0' + id.lib].push_back(e);
                count++;
            }
        }
        (void)count;
        fs_patched += patch_fs_loads(image, m.self.ph, base);
        auto it = m.dyn.tags.find(12);
        table.push_back({base, (u64)modsize, base + (it == m.dyn.tags.end() ? 0 : it->second),
                         tls ? base + tls->vaddr : 0, tls ? tls->memsz : 0, tls ? tls->filesz : 0, module_id, fname});
        base = (u64)(((u128)base + modsize + 65535) & ~(u128)65535);
    }

    std::vector<Binding> bindings;
    u64 unresolved = 0;
    for (size_t k = 0; k < id_list.size(); k++) {
        const Ident& id = id_list[k];
        u64 index = id_index[k];
        const Export* found = nullptr;
        auto e = exports.find(id.key());
        if (id.valid && e != exports.end()) found = &e->second;
        if (!found && id.valid && id.lib == "libSceLibcInternal") {
            auto c = by_nid.find(id.n + '\0' + "libc");
            if (c != by_nid.end() && c->second.size() == 1) found = &c->second[0];
        }
        if (found) bindings.push_back({index, found->address, found->kind});
        else unresolved++;
    }

    const Phdr* main_tls = find_phdr(main.self.ph, PT_TLS);
    u64 mt[4] = {0, 0, 0, 0};
    if (main_tls) {
        mt[0] = main_tls->vaddr; mt[1] = main_tls->filesz; mt[2] = main_tls->memsz; mt[3] = main_tls->align;
        if (main_tls->filesz > main_tls->memsz || main_tls->memsz > 1024 * 1024 || (u128)main_tls->vaddr + main_tls->filesz > size)
            raise("ValueError", "unsupported eboot TLS layout");
    }
    const Phdr& procparam = require_phdr(main.self.ph, PT_SCE_PROCPARAM);

    std::string path = path_join(out_dir, "boot-linked.bin");
    OutFile out(path);
    Bytes head;
    auto q = [&](u64 v) { head.insert(head.end(), (u8*)&v, (u8*)&v + 8); };
    head.insert(head.end(), {'B', 'B', 'P', 'R', 'O', 'B', 'E', '5'});
    q(image.size()); q(entry); q(segments.size()); q(relocs.size()); q(names.size()); q(flags);
    q(procparam.vaddr);
    for (u64 v : mt) q(v);
    q(table.size());
    for (auto& t : table) { q(t.base); q(t.size); q(t.init); q(t.tls_address); q(t.tls_memsz); q(t.tls_filesz); q(t.tls_module); }
    q(bindings.size());
    for (auto& b : bindings) { q(b.index); q(b.address); q(b.kind); }
    for (auto& s : segments) { q(s[0]); q(s[1]); q(s[2]); }
    for (auto& n : names) {
        if (n.size() >= 128) raise("ValueError", "name too long");
        head.insert(head.end(), n.begin(), n.end());
        head.insert(head.end(), 128 - n.size(), 0);
    }
    for (auto& r : relocs) { q(r.target); q(r.kind); q((u64)checked_q_signed(r.value)); q((u64)r.addend); }
    out.write(head.data(), head.size());
    out.write(image.data(), image.size());
    out.close();

    std::string summary;
    for (auto& t : table) {
        if (!summary.empty()) summary += ", ";
        summary += t.file + "@" + hex(t.base);
    }
    std::printf("Linked modules: %s; %zu native bindings, %llu imports left to the host runtime, fs->gs patched=%llu\n",
                summary.c_str(), bindings.size(), (unsigned long long)unresolved, (unsigned long long)fs_patched);
    std::fflush(stdout);
}
