/* Minimal PNG writer: stored (uncompressed) deflate blocks, no library needed.
 *
 * Copyright (c) 2026 Reda Cherif-Touil
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "gs_xts.h"
#include <stdio.h>
#include <string.h>

static uint32_t png_crc(const uint8_t *p, size_t n, uint32_t c = 0xFFFFFFFFu)
{
    for (size_t i = 0; i < n; i++) {
        c ^= p[i];
        for (int k = 0; k < 8; k++)
            c = (c >> 1) ^ (0xEDB88320u & (uint32_t)(-(int32_t)(c & 1)));
    }
    return c;
}

static void be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v;
}

static void chunk(FILE *f, const char *tag, const uint8_t *data, size_t n)
{
    uint8_t len[4]; be32(len, (uint32_t)n); fwrite(len, 1, 4, f);
    fwrite(tag, 1, 4, f);
    if (n) fwrite(data, 1, n, f);
    uint32_t c = png_crc((const uint8_t *)tag, 4);
    if (n) c = png_crc(data, n, c);
    uint8_t crc[4]; be32(crc, ~c); fwrite(crc, 1, 4, f);
}

bool gs_write_png(const char *path, const uint8_t *rgba, int w, int h)
{
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    static const uint8_t sig[8] = { 137, 'P', 'N', 'G', 13, 10, 26, 10 };
    fwrite(sig, 1, 8, f);

    uint8_t ihdr[13];
    be32(ihdr, (uint32_t)w); be32(ihdr + 4, (uint32_t)h);
    ihdr[8] = 8; ihdr[9] = 6; ihdr[10] = ihdr[11] = ihdr[12] = 0;   /* 8-bit RGBA */
    chunk(f, "IHDR", ihdr, sizeof(ihdr));

    /* raw scanlines, each with a "no filter" byte in front */
    std::vector<uint8_t> raw((size_t)h * (1 + (size_t)w * 4));
    for (int y = 0; y < h; y++) {
        uint8_t *row = &raw[(size_t)y * (1 + (size_t)w * 4)];
        row[0] = 0;
        memcpy(row + 1, rgba + (size_t)y * w * 4, (size_t)w * 4);
    }
    /* zlib wrapper around stored deflate blocks */
    std::vector<uint8_t> z;
    z.push_back(0x78); z.push_back(0x01);
    for (size_t off = 0; off < raw.size(); ) {
        size_t n = raw.size() - off; if (n > 65535) n = 65535;
        bool last = off + n >= raw.size();
        z.push_back(last ? 1 : 0);
        z.push_back((uint8_t)(n & 255)); z.push_back((uint8_t)(n >> 8));
        z.push_back((uint8_t)(~n & 255)); z.push_back((uint8_t)((~n >> 8) & 255));
        z.insert(z.end(), raw.begin() + off, raw.begin() + off + n);
        off += n;
    }
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < raw.size(); i++) { a = (a + raw[i]) % 65521; b = (b + a) % 65521; }
    uint8_t ad[4]; be32(ad, (b << 16) | a);
    z.insert(z.end(), ad, ad + 4);
    chunk(f, "IDAT", z.data(), z.size());
    chunk(f, "IEND", NULL, 0);
    fclose(f);
    return true;
}
