/* Copyright (c) 2026 Reda Cherif-Touil
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "gs_xts.h"
#include "gs_data.h"
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <ctype.h>

#ifdef _WIN32
#include <direct.h>
#define gs_mkdir(p) _mkdir(p)
#else
#include <sys/stat.h>
#define gs_mkdir(p) mkdir(p, 0755)
#endif

static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static int16_t  rds16(const uint8_t *p) { return (int16_t)(p[0] | (p[1] << 8)); }

/* The texture index written in a sprite record belongs to the NEXT record, so a
 * sprite sits in the texture the sprite before it declared. Without this, eleven
 * sprites — four rank badges among them — come out of the wrong texture. */
static void read_sprites(const uint8_t *p, size_t n, std::vector<GsSprite> &out)
{
    if (n < 20) return;
    uint32_t count = rd32(p + 16);
    if (count > 4096) return;
    out.resize(count);
    for (uint32_t i = 0; i < count; i++) {
        size_t o = 0x20 + (size_t)i * 32;
        if (o + 32 > n) { out.resize(i); break; }
        out[i].tex = (int)rd32(p + o + 24);
        for (int k = 0; k < 4; k++) out[i].rect[k] = rds16(p + o + 16 + k * 2);
    }
    for (size_t i = out.size(); i-- > 1; ) out[i].tex = out[i - 1].tex;
}

static void read_textures(const uint8_t *p, size_t n, std::vector<GsTexture> &out)
{
    uint32_t xp = rd32(p + 4);
    if (xp + 12 > n || memcmp(p + xp, "XPR0", 4) != 0) return;
    uint32_t hdr = rd32(p + xp + 8);
    size_t data0 = xp + hdr;
    for (size_t off = xp + 12; off + 20 <= xp + hdr; off += 20) {
        uint32_t common = rd32(p + off), dofs = rd32(p + off + 4), fmt = rd32(p + off + 12);
        if (common == 0xADADADADu || fmt == 0xADADADADu || (common == 0 && fmt == 0)) break;
        GsTexture t;
        t.colour = (int)((fmt >> 8) & 0xFF);
        t.w = 1 << ((fmt >> 20) & 0xF);
        t.h = 1 << ((fmt >> 24) & 0xF);
        t.at = data0 + dofs;
        out.push_back(t);
    }
}

static int max_texture_wanted()
{
    int m = 0;
    for (int i = 0; i < GS_NCOSTUMES; i++)
        m = std::max(m, std::max(GS_COSTUMES[i].tex_jp, std::max(GS_COSTUMES[i].tex_world, GS_COSTUMES[i].tex_locked)));
    for (int i = 0; i < GS_NWEAPONS; i++)
        m = std::max(m, std::max(GS_WEAPONS[i].tex_jp, std::max(GS_WEAPONS[i].tex_world, GS_WEAPONS[i].tex_locked)));
    return m;
}

static bool badges_look_right(const std::vector<GsSprite> &spr)
{
    for (int i = 0; i < 17; i++) {
        int id = GS_BADGE_SPRITE[i];
        if (id >= (int)spr.size()) return false;
        const int16_t *r = spr[id].rect;
        if (r[2] - r[0] != 30 || r[3] - r[1] != 20) return false;
    }
    return true;
}

/* ---------------------------------------------------------------- the game's
 * own filesystem. A Chihiro game image is an FATX partition: "FATX" at byte 0,
 * clusters of a few sectors, a FAT at 0x1000 and the data right after it, with
 * the root directory in cluster 1. A directory entry is 64 bytes: name length,
 * attributes, the name on 42, the first cluster at 0x2C, the size at 0x30.
 * Reading it lets us open /media/spr_card.xts by name instead of hunting for it
 * through half a gigabyte. */

struct Fatx {
    FILE *f = NULL;
    uint32_t cluster = 0, data0 = 0, nclust = 0, width = 2;
    std::vector<uint8_t> fat;

    bool open(FILE *file, long long size)
    {
        uint8_t h[16];
        if (fseek(file, 0, SEEK_SET) != 0 || fread(h, 1, sizeof h, file) != sizeof h) return false;
        if (memcmp(h, "FATX", 4) != 0) return false;
        f = file;
        cluster = rd32(h + 8) * 512;
        if (cluster < 512 || cluster > (1u << 20)) return false;
        nclust = (uint32_t)((size - 0x1000) / cluster) + 1;
        width = nclust < 0xFFF0 ? 2 : 4;
        uint32_t fatsize = ((nclust * width) + 4095) / 4096 * 4096;
        fat.resize(fatsize);
        fseek(f, 0x1000, SEEK_SET);
        if (fread(fat.data(), 1, fat.size(), f) != fat.size()) return false;
        /* The FAT area is padded, and by how much varies; the root directory is
         * the first thing after it that reads like a directory entry. */
        for (uint32_t pad = 0; pad <= 0x10000; pad += 0x1000) {
            data0 = 0x1000 + fatsize + pad;
            uint8_t e[64];
            fseek(f, (long)data0, SEEK_SET);
            if (fread(e, 1, sizeof e, f) != sizeof e) return false;
            if (e[0] >= 1 && e[0] <= 42 && (e[1] & ~0x37) == 0) return true;
        }
        return false;
    }
    uint32_t next(uint32_t c) const
    {
        if (width == 2) return (size_t)c * 2 + 1 < fat.size() ? rd16(&fat[(size_t)c * 2]) : 0;
        return (size_t)c * 4 + 3 < fat.size() ? rd32(&fat[(size_t)c * 4]) : 0;
    }
    /* Reads a file, or a directory when size is 0. */
    bool read(uint32_t first, uint32_t size, std::vector<uint8_t> &out)
    {
        const uint32_t end = width == 2 ? 0xFFF8u : 0xFFFFFFF8u;
        out.clear();
        for (uint32_t c = first, guard = 0; c && c < end && guard < 200000; c = next(c), guard++) {
            size_t was = out.size();
            out.resize(was + cluster);
            if (fseek(f, (long)(data0 + (long long)(c - 1) * cluster), SEEK_SET) != 0 ||
                fread(&out[was], 1, cluster, f) != cluster) { out.clear(); return false; }
            if (size && out.size() >= size) break;
            if (!size && out.size() > (16u << 20)) break;
        }
        if (size) out.resize(size);
        return !out.empty();
    }
    /* Looks for one file by name, anywhere in the tree, and reads it. */
    bool find(const char *name, std::vector<uint8_t> &out, uint32_t dir = 1, int depth = 0)
    {
        if (depth > 4) return false;
        std::vector<uint8_t> d;
        if (!read(dir, 0, d)) return false;
        for (size_t o = 0; o + 64 <= d.size(); o += 64) {
            const uint8_t n = d[o];
            if (n == 0x00 || n == 0xFF) break;
            if (n == 0xE5 || n > 42) continue;
            std::string nm((const char *)&d[o + 2], n);
            const uint32_t first = rd32(&d[o + 0x2C]), size = rd32(&d[o + 0x30]);
            if (d[o + 1] & 0x10) {
                if (first && find(name, out, first, depth + 1)) return true;
            } else if (nm == name) {
                return read(first, size, out);
            }
        }
        return false;
    }
};

/* Pulls one named sprite archive out of a game image. */
static bool archive_by_name(const char *path, const char *name, GsArchive *out)
{
    FILE *f = fopen(path, "rb");
    if (!f) return false;
#ifdef _WIN32
    _fseeki64(f, 0, SEEK_END); long long size = _ftelli64(f);
#else
    fseeko(f, 0, SEEK_END); long long size = ftello(f);
#endif
    Fatx fx;
    bool ok = fx.open(f, size) && fx.find(name, out->data);
    fclose(f);
    if (!ok || out->data.size() < 32) { out->data.clear(); return false; }
    read_textures(out->data.data(), out->data.size(), out->tex);
    read_sprites(out->data.data(), out->data.size(), out->spr);
    out->source = path;
    if (!out->ok()) { *out = GsArchive(); return false; }
    return true;
}

bool gs_find_hud_archive(const char *path, GsArchive *out)
{
    return archive_by_name(path, "spr_anime_game.xts", out);
}

bool gs_find_archive(const char *path, GsArchive *out, std::string *err,
                     void (*progress)(float, void *), void *user)
{
    /* The quick way first: a game image is a filesystem, and the archive has a
     * name. Anything else - a loose .xts, a differently packed image - still
     * goes through the scan below. */
    if (archive_by_name(path, "spr_card.xts", out)) {
        if (progress) progress(1.0f, user);
        return true;
    }
    FILE *f = fopen(path, "rb");
    if (!f) { *err = "Could not open that file."; return false; }
#ifdef _WIN32
    _fseeki64(f, 0, SEEK_END); long long size = _ftelli64(f); _fseeki64(f, 0, SEEK_SET);
#else
    fseeko(f, 0, SEEK_END); long long size = ftello(f); fseeko(f, 0, SEEK_SET);
#endif
    const size_t CHUNK = 8u << 20, BACK = 256u << 10;
    std::vector<uint8_t> buf(CHUNK), head;
    bool found = false;
    for (long long pos = 0; pos < size && !found; pos += (long long)CHUNK - 3) {
        if (progress) progress((float)((double)pos / (double)size), user);
#ifdef _WIN32
        _fseeki64(f, pos, SEEK_SET);
#else
        fseeko(f, pos, SEEK_SET);
#endif
        size_t got = fread(buf.data(), 1, CHUNK, f);
        for (size_t i = 0; i + 3 < got && !found; i++) {
            const uint8_t *hit = (const uint8_t *)memchr(buf.data() + i, 'X', got - i - 3);
            if (!hit) break;
            i = (size_t)(hit - buf.data());
            if (buf[i+1] != 'P' || buf[i+2] != 'R' || buf[i+3] != '0') continue;
            long long x = pos + (long long)i;
            long long back = x - (long long)BACK; if (back < 0) back = 0;
            head.resize((size_t)(x - back) + 8);
#ifdef _WIN32
            _fseeki64(f, back, SEEK_SET);
#else
            fseeko(f, back, SEEK_SET);
#endif
            if (fread(head.data(), 1, head.size(), f) != head.size()) continue;
            for (long long p = (long long)head.size() - 8 - 16; p >= 0; p -= 4) {
                if (rd32(&head[(size_t)p]) != 0) continue;
                if ((long long)rd32(&head[(size_t)p + 4]) != x - (back + p)) continue;
                uint32_t ntex = rd32(&head[(size_t)p + 8]);
                if ((int)ntex > max_texture_wanted()) {
                    std::vector<GsSprite> spr;
                    read_sprites(&head[(size_t)p], head.size() - (size_t)p, spr);
                    if (badges_look_right(spr)) {
                        long long start = back + p, total = rd32(&head[head.size() - 4]);
                        out->data.resize((size_t)(x + total - start));
#ifdef _WIN32
                        _fseeki64(f, start, SEEK_SET);
#else
                        fseeko(f, start, SEEK_SET);
#endif
                        if (fread(out->data.data(), 1, out->data.size(), f) == out->data.size())
                            found = true;
                    }
                }
                break;
            }
        }
    }
    fclose(f);
    if (!found) { *err = "No Ghost Squad artwork in that file."; return false; }
    read_textures(out->data.data(), out->data.size(), out->tex);
    read_sprites(out->data.data(), out->data.size(), out->spr);
    out->source = path;
    if (!out->ok()) { *err = "The artwork in that file could not be read."; return false; }
    if (progress) progress(1.0f, user);
    return true;
}

bool gs_decode_texture(const GsArchive &a, int index, std::vector<uint8_t> &rgba, int *w, int *h)
{
    if (index < 0 || index >= (int)a.tex.size()) return false;
    const GsTexture &t = a.tex[index];
    const bool dxt1 = t.colour == 0x0C;
    const int stride = dxt1 ? 8 : 16;
    const int bw = t.w > 4 ? t.w / 4 : 1, bh = t.h > 4 ? t.h / 4 : 1;
    if (t.at + (size_t)bw * bh * stride > a.data.size()) return false;
    rgba.assign((size_t)t.w * t.h * 4, 0);
    const uint8_t *base = a.data.data();
    int pal[12]; int atab[8];
    for (int blk = 0; blk < bw * bh; blk++) {
        const uint8_t *o = base + t.at + (size_t)blk * stride;
        const int bx = (blk % bw) * 4, by = (blk / bw) * 4, coff = dxt1 ? 0 : 8;
        const uint16_t c0 = rd16(o + coff), c1 = rd16(o + coff + 2);
        const uint32_t bits = rd32(o + coff + 4);
        const int r0 = ((c0 >> 11) & 31) * 255 / 31, g0 = ((c0 >> 5) & 63) * 255 / 63, b0 = (c0 & 31) * 255 / 31;
        const int r1 = ((c1 >> 11) & 31) * 255 / 31, g1 = ((c1 >> 5) & 63) * 255 / 63, b1 = (c1 & 31) * 255 / 31;
        pal[0] = r0; pal[1] = g0; pal[2] = b0; pal[3] = r1; pal[4] = g1; pal[5] = b1;
        if (dxt1 && c0 <= c1) {
            pal[6] = (r0 + r1) / 2; pal[7] = (g0 + g1) / 2; pal[8] = (b0 + b1) / 2;
            pal[9] = pal[10] = pal[11] = 0;
        } else {
            pal[6] = (2 * r0 + r1) / 3; pal[7] = (2 * g0 + g1) / 3; pal[8] = (2 * b0 + b1) / 3;
            pal[9] = (r0 + 2 * r1) / 3; pal[10] = (g0 + 2 * g1) / 3; pal[11] = (b0 + 2 * b1) / 3;
        }
        uint32_t alo = 0, ahi = 0;
        if (t.colour == 0x0E) { alo = rd32(o); ahi = rd32(o + 4); }
        else if (t.colour == 0x0F) {
            const int a0 = o[0], a1 = o[1];
            atab[0] = a0; atab[1] = a1;
            if (a0 > a1) for (int i = 1; i < 7; i++) atab[i + 1] = ((7 - i) * a0 + i * a1) / 7;
            else { for (int i = 1; i < 5; i++) atab[i + 1] = ((5 - i) * a0 + i * a1) / 5; atab[6] = 0; atab[7] = 255; }
            alo = rd32(o + 2); ahi = rd16(o + 6);
        }
        for (int k = 0; k < 16; k++) {
            const int x = bx + (k & 3), y = by + (k >> 2);
            if (x >= t.w || y >= t.h) continue;
            int alpha = 255;
            if (t.colour == 0x0E)
                alpha = (int)(((k < 8 ? alo >> (k * 4) : ahi >> ((k - 8) * 4)) & 0xF) * 17);
            else if (t.colour == 0x0F) {
                const int bp = k * 3;
                alpha = atab[bp + 3 <= 32 ? (alo >> bp) & 7 : bp >= 32 ? (ahi >> (bp - 32)) & 7
                                          : ((alo >> 30) | (ahi << 2)) & 7];
            } else if (c0 <= c1 && ((bits >> (k * 2)) & 3) == 3) alpha = 0;
            const int c = (int)((bits >> (k * 2)) & 3) * 3;
            uint8_t *p = &rgba[((size_t)(t.h - 1 - y) * t.w + x) * 4];   /* rows are stored bottom-up */
            p[0] = (uint8_t)pal[c]; p[1] = (uint8_t)pal[c + 1]; p[2] = (uint8_t)pal[c + 2]; p[3] = (uint8_t)alpha;
        }
    }
    *w = t.w; *h = t.h;
    return true;
}

void gs_painted_box(const std::vector<uint8_t> &rgba, int w, int h, int box[4])
{
    int x0 = w, y0 = h, x1 = 0, y1 = 0;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            if (rgba[((size_t)y * w + x) * 4 + 3] > 8) {
                if (x < x0) x0 = x; if (y < y0) y0 = y;
                if (x >= x1) x1 = x + 1; if (y >= y1) y1 = y + 1;
            }
    if (x1 <= x0) { box[0] = 0; box[1] = 0; box[2] = w; box[3] = h; }
    else { box[0] = x0; box[1] = y0; box[2] = x1; box[3] = y1; }
}

static std::vector<uint8_t> cut(const std::vector<uint8_t> &src, int w, int, const int box[4])
{
    const int cw = box[2] - box[0], ch = box[3] - box[1];
    std::vector<uint8_t> out((size_t)cw * ch * 4);
    for (int y = 0; y < ch; y++)
        memcpy(&out[(size_t)y * cw * 4], &src[((size_t)(y + box[1]) * w + box[0]) * 4], (size_t)cw * 4);
    return out;
}

static std::string slug(const char *name)
{
    std::string s;
    for (const char *p = name; *p; p++) s += (*p == ' ') ? '-' : (char)tolower((unsigned char)*p);
    return s;
}

/* Creates every level of the path, not only the last one: the assets folder
 * itself may not exist yet. */
static void make_dirs(const std::string &p)
{
    std::string acc;
    for (size_t i = 0; i < p.size(); i++) {
        acc += p[i];
        if ((p[i] == '/' || p[i] == '\\') && acc.size() > 1) gs_mkdir(acc.c_str());
    }
    gs_mkdir(p.c_str());
}

bool gs_cut_sprite(const GsArchive &a, int sprite, std::vector<uint8_t> &rgba, int *w, int *h)
{
    if (sprite < 0 || sprite >= (int)a.spr.size()) return false;
    const GsSprite &q = a.spr[sprite];
    int box[4] = { q.rect[0], q.rect[1], q.rect[2], q.rect[3] };
    if (box[2] <= box[0] || box[3] <= box[1]) return false;
    std::vector<uint8_t> px; int tw, th;
    if (!gs_decode_texture(a, q.tex, px, &tw, &th)) return false;
    if (box[2] > tw || box[3] > th) return false;
    rgba = cut(px, tw, th, box);
    *w = box[2] - box[0]; *h = box[3] - box[1];
    return true;
}

/* The name plate and the icon the game paints in its own HUD, for the items the
 * locker has no card for. Whatever is missing is simply not written; the
 * interface falls back to the name in text. */
bool gs_export_hidden(const GsArchive &hud, const char *dir, std::string *err)
{
    if (!hud.ok()) { *err = "No HUD artwork in that file."; return false; }
    const std::string root = std::string(dir) + "/hidden";
    make_dirs(root);
    std::vector<uint8_t> px; int w, h;
    int written = 0;
    for (int i = 0; i < GS_NHIDDEN_W; i++) {
        const GsHidden &it = GS_HIDDEN_WEAPONS[i];
        const int want[2] = { it.plate, it.icon };
        const char *what[2] = { "name", "icon" };
        for (int k = 0; k < 2; k++) {
            if (want[k] < 0 || !gs_cut_sprite(hud, want[k], px, &w, &h)) continue;
            char path[256];
            snprintf(path, sizeof path, "%s/%02d-%s.png", root.c_str(), it.idx, what[k]);
            if (gs_write_png(path, px.data(), w, h)) written++;
        }
    }
    if (!written) { *err = "The HUD artwork could not be read."; return false; }
    return true;
}

bool gs_export_assets(const GsArchive &a, const char *dir, bool japanese, std::string *err)
{
    std::string root = dir;
    make_dirs(root);
    for (const char *sub : { "costumes", "weapons", "badges", "icons" })
        make_dirs(root + "/" + sub);

    std::vector<uint8_t> px; int w, h, box[4];
    auto card = [&](int tex, const std::string &path) {
        if (!gs_decode_texture(a, tex, px, &w, &h)) return false;
        gs_painted_box(px, w, h, box);
        std::vector<uint8_t> c = cut(px, w, h, box);
        return gs_write_png(path.c_str(), c.data(), box[2] - box[0], box[3] - box[1]);
    };
    for (int pass = 0; pass < 2; pass++) {
        const GsItem *list = pass ? GS_WEAPONS : GS_COSTUMES;
        const int n = pass ? GS_NWEAPONS : GS_NCOSTUMES;
        const char *kind = pass ? "weapons" : "costumes";
        for (int i = 0; i < n; i++) {
            char base[256];
            snprintf(base, sizeof base, "%s/%s/%02d-%s", root.c_str(), kind, list[i].bit, slug(list[i].name).c_str());
            if (!card(japanese ? list[i].tex_jp : list[i].tex_world, std::string(base) + ".png") ||
                !card(list[i].tex_locked, std::string(base) + "-locked.png")) {
                *err = "Could not write the pictures. Is the folder writable?";
                return false;
            }
        }
    }
    for (int cls = 0; cls < 17; cls++) {
        const GsSprite &s = a.spr[GS_BADGE_SPRITE[cls]];
        if (!gs_decode_texture(a, s.tex, px, &w, &h)) return false;
        int b[4] = { s.rect[0], s.rect[1], s.rect[2], s.rect[3] };
        std::vector<uint8_t> c = cut(px, w, h, b);
        char path[256]; snprintf(path, sizeof path, "%s/badges/%02d.png", root.c_str(), cls);
        gs_write_png(path, c.data(), b[2] - b[0], b[3] - b[1]);
    }
    {   /* the menu plates, whole, for the interface to stretch as it likes */
        gs_mkdir((root + "/buttons").c_str());
        const int want[1] = { GS_PLATE_SPRITE };
        const char *names[1] = { "plate" };
        for (int i = 0; i < 1; i++) {
            if (want[i] >= (int)a.spr.size()) continue;
            const GsSprite &q = a.spr[want[i]];
            if (!gs_decode_texture(a, q.tex, px, &w, &h)) continue;
            int bx[4] = { q.rect[0], q.rect[1], q.rect[2], q.rect[3] };
            std::vector<uint8_t> c = cut(px, w, h, bx);
            char path[256]; snprintf(path, sizeof path, "%s/buttons/%s.png", root.c_str(), names[i]);
            gs_write_png(path, c.data(), bx[2] - bx[0], bx[3] - bx[1]);
        }
    }

    const GsSprite &panel = a.spr[GS_ICON_SPRITE];
    if (gs_decode_texture(a, panel.tex, px, &w, &h)) {
        const int *boxes[2] = { GS_ICON_GUN, GS_ICON_SHIRT };
        const char *names[2] = { "gun", "shirt" };
        for (int i = 0; i < 2; i++) {
            int b[4] = { boxes[i][0] + panel.rect[0], boxes[i][1] + panel.rect[1],
                         boxes[i][2] + panel.rect[0], boxes[i][3] + panel.rect[1] };
            std::vector<uint8_t> c = cut(px, w, h, b);
            char path[256]; snprintf(path, sizeof path, "%s/icons/%s.png", root.c_str(), names[i]);
            gs_write_png(path, c.data(), b[2] - b[0], b[3] - b[1]);
        }
    }
    return true;
}
