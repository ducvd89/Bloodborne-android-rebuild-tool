// SPDX-License-Identifier: GPL-2.0-or-later
// SHA-256 / SHA-1 (RFC 6234); the NDK has no libcrypto.
#include "hash.h"

#include <cstdio>

namespace {
inline u32 rol(u32 x, int n) { return (x << n) | (x >> (32 - n)); }
inline u32 ror(u32 x, int n) { return (x >> n) | (x << (32 - n)); }
inline u32 be32(const u8* p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }

const u32 K256[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

void sha256_block(u32 h[8], const u8* p) {
    u32 w[64];
    for (int i = 0; i < 16; i++) w[i] = be32(p + 4 * i);
    for (int i = 16; i < 64; i++) {
        u32 s0 = ror(w[i - 15], 7) ^ ror(w[i - 15], 18) ^ (w[i - 15] >> 3);
        u32 s1 = ror(w[i - 2], 17) ^ ror(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    u32 a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; i++) {
        u32 t1 = hh + (ror(e, 6) ^ ror(e, 11) ^ ror(e, 25)) + ((e & f) ^ (~e & g)) + K256[i] + w[i];
        u32 t2 = (ror(a, 2) ^ ror(a, 13) ^ ror(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

void pad_tail(u8* tail, size_t* tail_len, size_t rem_off, const u8* d, size_t n) {
    // tail holds the last partial block + padding + 64-bit big-endian bit length.
    size_t rem = n - rem_off;
    std::memcpy(tail, d + rem_off, rem);
    tail[rem] = 0x80;
    size_t total = rem + 1 + 8 <= 64 ? 64 : 128;
    std::memset(tail + rem + 1, 0, total - rem - 1);
    u64 bits = (u64)n * 8;
    for (int i = 0; i < 8; i++) tail[total - 1 - i] = (u8)(bits >> (8 * i));
    *tail_len = total;
}
}  // namespace

std::string sha256_hex(View d) {
    u32 h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    size_t full = d.n & ~(size_t)63;
    for (size_t i = 0; i < full; i += 64) sha256_block(h, d.p + i);
    u8 tail[128];
    size_t tl;
    pad_tail(tail, &tl, full, d.p, d.n);
    for (size_t i = 0; i < tl; i += 64) sha256_block(h, tail + i);
    char out[65];
    for (int i = 0; i < 8; i++) std::snprintf(out + 8 * i, 9, "%08x", h[i]);
    return out;
}

std::array<u8, 20> sha1(const u8* d, size_t n) {
    u32 h[5] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0};
    auto block = [&](const u8* p) {
        u32 w[80];
        for (int i = 0; i < 16; i++) w[i] = be32(p + 4 * i);
        for (int i = 16; i < 80; i++) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        u32 a = h[0], b = h[1], c = h[2], dd = h[3], e = h[4];
        for (int i = 0; i < 80; i++) {
            u32 f, k;
            if (i < 20) { f = (b & c) | (~b & dd); k = 0x5a827999; }
            else if (i < 40) { f = b ^ c ^ dd; k = 0x6ed9eba1; }
            else if (i < 60) { f = (b & c) | (b & dd) | (c & dd); k = 0x8f1bbcdc; }
            else { f = b ^ c ^ dd; k = 0xca62c1d6; }
            u32 t = rol(a, 5) + f + e + k + w[i];
            e = dd; dd = c; c = rol(b, 30); b = a; a = t;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += dd; h[4] += e;
    };
    size_t full = n & ~(size_t)63;
    for (size_t i = 0; i < full; i += 64) block(d + i);
    u8 tail[128];
    size_t tl;
    pad_tail(tail, &tl, full, d, n);
    for (size_t i = 0; i < tl; i += 64) block(tail + i);
    std::array<u8, 20> r;
    for (int i = 0; i < 5; i++)
        for (int j = 0; j < 4; j++) r[4 * i + j] = (u8)(h[i] >> (24 - 8 * j));
    return r;
}
