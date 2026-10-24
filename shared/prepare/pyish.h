// SPDX-License-Identifier: GPL-2.0-or-later
// Small helpers that keep the C++ port of scripts/prepare.py & co. close to the Python original:
// bounds-checked spans with the same error text, Python-style exceptions, UTF-8 decoding rules.
#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i64 = int64_t;
using u128 = unsigned __int128;
using i128 = __int128;
using Bytes = std::vector<u8>;
static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__, "little-endian hosts only");

// A Python exception that reached the top level: printed as "Kind: message" (the last line of a
// traceback), exit status 1.
struct PyError {
    std::string kind, msg;
};
[[noreturn]] void raise(std::string kind, std::string msg = {});
[[noreturn]] void raise_errno(int err, const std::string& path);  // OSError family, like Python's

std::string hex(u128 v);                 // f'{v:#x}'
std::string commas(u64 v);               // f'{v:,}'
std::string py_repr_str(const std::string& s);

struct View {
    const u8* p = nullptr;
    size_t n = 0;
};
inline View view(const Bytes& b) { return {b.data(), b.size()}; }

// data[offset:offset+size] with prepare.py's span() check.
inline View span(View d, u128 off, u128 size) {
    if (off + size > d.n)
        raise("ValueError", "out of bounds: " + hex(off) + "+" + hex(size) + " / " + hex(d.n));
    return {d.p + (size_t)off, (size_t)size};
}

// Sequential little-endian reader over a span (struct.unpack with '<').
struct Cur {
    const u8* p;
    template <class T> T get() {
        T v;
        std::memcpy(&v, p, sizeof v);
        p += sizeof v;
        return v;
    }
    u8 u8_() { return get<u8>(); }
    u16 u16_() { return get<u16>(); }
    u32 u32_() { return get<u32>(); }
    u64 u64_() { return get<u64>(); }
    i64 i64_() { return get<i64>(); }
};
inline Cur unpack(View d, u128 off, size_t calcsize) { return Cur{span(d, off, calcsize).p}; }

// bytes.index(0, start) of prepare.py's string readers; ValueError('subsection not found').
size_t index_zero(View d, u128 start);
// d[start:end].decode('ascii') / strict utf-8 / utf-8 with errors='replace'.
std::string decode_ascii(View d);
std::string decode_utf8_strict(View d);
std::string decode_utf8_replace(View d);

// struct.pack_into range checks.
u64 checked_q_unsigned(i128 v);
i64 checked_q_signed(i128 v);

// pathlib-like helpers.
std::string path_norm(const std::string& p);
std::string path_join(const std::string& a, const std::string& b);
