// SPDX-License-Identifier: GPL-2.0-or-later
#include "pyish.h"

#include <cerrno>
#include <cstdio>

void raise(std::string kind, std::string msg) { throw PyError{std::move(kind), std::move(msg)}; }

void raise_errno(int err, const std::string& path) {
    const char* kind = "OSError";
    switch (err) {
    case ENOENT: kind = "FileNotFoundError"; break;
    case EEXIST: kind = "FileExistsError"; break;
    case EACCES:
    case EPERM: kind = "PermissionError"; break;
    case EISDIR: kind = "IsADirectoryError"; break;
    case ENOTDIR: kind = "NotADirectoryError"; break;
    }
    raise(kind, "[Errno " + std::to_string(err) + "] " + std::strerror(err) + ": " + py_repr_str(path));
}

std::string hex(u128 v) {
    if (!v) return "0x0";
    std::string s;
    while (v) {
        s.insert(s.begin(), "0123456789abcdef"[(unsigned)(v & 15)]);
        v >>= 4;
    }
    return "0x" + s;
}

std::string commas(u64 v) {
    std::string s = std::to_string(v), r;
    for (size_t i = 0; i < s.size(); i++) {
        if (i && (s.size() - i) % 3 == 0) r += ',';
        r += s[i];
    }
    return r;
}

std::string py_repr_str(const std::string& s) {
    bool sq = s.find('\'') != std::string::npos, dq = s.find('"') != std::string::npos;
    char q = (sq && !dq) ? '"' : '\'';
    std::string r(1, q);
    for (unsigned char c : s) {
        if (c == (unsigned char)q || c == '\\') { r += '\\'; r += (char)c; }
        else if (c == '\n') r += "\\n";
        else if (c == '\r') r += "\\r";
        else if (c == '\t') r += "\\t";
        else if (c < 0x20 || c == 0x7f) { char b[8]; std::snprintf(b, sizeof b, "\\x%02x", c); r += b; }
        else r += (char)c;
    }
    return r + q;
}

size_t index_zero(View d, u128 start) {
    if (start <= d.n) {
        const void* z = d.n > start ? std::memchr(d.p + (size_t)start, 0, d.n - (size_t)start) : nullptr;
        if (z) return (const u8*)z - d.p;
    }
    raise("ValueError", "subsection not found");
}

std::string decode_ascii(View d) {
    for (size_t i = 0; i < d.n; i++)
        if (d.p[i] >= 0x80) {
            char b[96];
            std::snprintf(b, sizeof b, "'ascii' codec can't decode byte 0x%02x in position %zu: ordinal not in range(128)",
                          d.p[i], i);
            raise("UnicodeDecodeError", b);
        }
    return std::string((const char*)d.p, d.n);
}

namespace {
// One step of Python's UTF-8 decoder: the length of the valid character at i, or 0 with the error
// range [i, *end) and its reason.
size_t utf8_step(View d, size_t i, size_t* end, const char** reason) {
    u8 b = d.p[i];
    if (b < 0x80) return 1;
    size_t need;
    u8 lo = 0x80, hi = 0xbf;
    if (b >= 0xc2 && b <= 0xdf) need = 1;
    else if (b >= 0xe0 && b <= 0xef) {
        need = 2;
        if (b == 0xe0) lo = 0xa0;
        if (b == 0xed) hi = 0x9f;
    } else if (b >= 0xf0 && b <= 0xf4) {
        need = 3;
        if (b == 0xf0) lo = 0x90;
        if (b == 0xf4) hi = 0x8f;
    } else {
        *end = i + 1;
        *reason = "invalid start byte";
        return 0;
    }
    for (size_t k = 1; k <= need; k++) {
        if (i + k >= d.n) {
            *end = d.n;
            *reason = "unexpected end of data";
            return 0;
        }
        u8 c = d.p[i + k];
        if (c < (k == 1 ? lo : 0x80) || c > (k == 1 ? hi : 0xbf)) {
            *end = i + k;
            *reason = "invalid continuation byte";
            return 0;
        }
    }
    return need + 1;
}
}  // namespace

std::string decode_utf8_strict(View d) {
    for (size_t i = 0; i < d.n;) {
        size_t end;
        const char* reason;
        size_t len = utf8_step(d, i, &end, &reason);
        if (!len) {
            char b[160];
            if (end == i + 1)
                std::snprintf(b, sizeof b, "'utf-8' codec can't decode byte 0x%02x in position %zu: %s", d.p[i], i, reason);
            else
                std::snprintf(b, sizeof b, "'utf-8' codec can't decode bytes in position %zu-%zu: %s", i, end - 1, reason);
            raise("UnicodeDecodeError", b);
        }
        i += len;
    }
    return std::string((const char*)d.p, d.n);
}

std::string decode_utf8_replace(View d) {
    std::string r;
    for (size_t i = 0; i < d.n;) {
        size_t end;
        const char* reason;
        size_t len = utf8_step(d, i, &end, &reason);
        if (len) {
            r.append((const char*)d.p + i, len);
            i += len;
        } else {
            r += "\xef\xbf\xbd";
            i = end;
        }
    }
    return r;
}

u64 checked_q_unsigned(i128 v) {
    if (v < 0 || v > (i128)UINT64_MAX)
        raise("struct.error", "'Q' format requires 0 <= number <= 18446744073709551615");
    return (u64)v;
}
i64 checked_q_signed(i128 v) {
    if (v < INT64_MIN || v > INT64_MAX)
        raise("struct.error", "'q' format requires -9223372036854775808 <= number <= 9223372036854775807");
    return (i64)v;
}

std::string path_norm(const std::string& p) {
    std::string r;
    size_t i = 0;
    bool abs = !p.empty() && p[0] == '/';
    while (i <= p.size()) {
        size_t j = p.find('/', i);
        if (j == std::string::npos) j = p.size();
        std::string part = p.substr(i, j - i);
        if (!part.empty() && part != ".") r += (r.empty() ? "" : "/") + part;
        i = j + 1;
    }
    if (abs) return "/" + r;
    return r.empty() ? "." : r;
}

std::string path_join(const std::string& a, const std::string& b) {
    std::string n = path_norm(a);
    if (n == ".") return path_norm(b);
    return path_norm(n + "/" + b);
}
