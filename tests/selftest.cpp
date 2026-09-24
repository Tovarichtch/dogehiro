/* Checks the importer on a real game image.
 *   dogehiro-selftest <game image> [reference RGBA of textures 0 and 318]
 *
 * Copyright (c) 2026 Reda Cherif-Touil
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "gs_data.h"
#include "gs_xts.h"
#include <stdio.h>
#include <string.h>
#include <vector>
#include <string>

static int fails = 0;
static void check(bool ok, const char *what, const char *detail = "")
{
    printf("  %-52s %s%s\n", what, ok ? "OK" : "FAILED", detail);
    if (!ok) fails++;
}

static std::vector<uint8_t> slurp(const char *p)
{
    std::vector<uint8_t> v;
    FILE *f = fopen(p, "rb");
    if (!f) return v;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    v.resize(n > 0 ? (size_t)n : 0);
    if (fread(v.data(), 1, v.size(), f) != v.size()) v.clear();
    fclose(f);
    return v;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <game image> [reference RGBA]\n", argv[0]);
        return 2;
    }
    GsArchive a;
    std::string e;
    char d[128];
    if (!gs_find_archive(argv[1], &a, &e, NULL, NULL)) {
        check(false, "card archive found in the game image", e.c_str());
        return 1;
    }
    snprintf(d, sizeof d, "   (%zu textures, %zu sprites)", a.tex.size(), a.spr.size());
    check(a.tex.size() == 329 && a.spr.size() == 398, "card archive recognised", d);

    std::vector<uint8_t> px; int w, h, box[4];
    check(gs_decode_texture(a, 0, px, &w, &h) && w == 128 && h == 128, "texture 0 decoded");
    gs_painted_box(px, w, h, box);
    snprintf(d, sizeof d, "   (%d,%d)-(%d,%d)", box[0], box[1], box[2], box[3]);
    check(box[0] == 20 && box[1] == 6 && box[2] == 108 && box[3] == 121, "painted area = 88x115 at (20,6)", d);

    /* optional: the reference decoder's RGBA of textures 0 and 318 */
    std::vector<uint8_t> ref = argc > 2 ? slurp(argv[2]) : std::vector<uint8_t>();
    if (ref.size() >= 65536) {
        check(memcmp(px.data(), ref.data(), 65536) == 0, "texture 0 identical to the reference");
        std::vector<uint8_t> px2;
        if (gs_decode_texture(a, 318, px2, &w, &h) && ref.size() >= 131072 + 262144)
            check(memcmp(px2.data(), ref.data() + 131072, 262144) == 0, "texture 318 (DXT1) identical");
    }
    bool badges = true;
    for (int i = 0; i < 17; i++) {
        const GsSprite &s = a.spr[GS_BADGE_SPRITE[i]];
        if (s.rect[2] - s.rect[0] != 30 || s.rect[3] - s.rect[1] != 20) badges = false;
    }
    check(badges, "the 17 badges are 30x20");
    printf("%s\n", fails ? "SOME CHECKS FAILED" : "all good");
    return fails ? 1 : 0;
}
