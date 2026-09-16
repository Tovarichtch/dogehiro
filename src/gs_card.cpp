/* Copyright (c) 2026 Reda Cherif-Touil
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "gs_card.h"
#include "gs_data.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

uint32_t gs_crc32_jam(const uint8_t *p, size_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        c ^= p[i];
        for (int k = 0; k < 8; k++)
            c = (c >> 1) ^ (0xEDB88320u & (uint32_t)(-(int32_t)(c & 1)));
    }
    return c;                       /* JAMCRC: the final complement is not applied */
}

uint32_t gs_u32(const GsCard *c, int off)
{
    const uint8_t *p = c->b + GS_SBHU + off;
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

void gs_set_u32(GsCard *c, int off, uint32_t v)
{
    uint8_t *p = c->b + GS_SBHU + off;
    p[0] = v & 255; p[1] = (v >> 8) & 255; p[2] = (v >> 16) & 255; p[3] = (v >> 24) & 255;
}

int gs_level_of(uint32_t exp)
{
    int n = 0;
    for (int i = 1; i <= 98; i++) if (GS_MILESTONE[i] <= exp) n++;
    return n;
}

int gs_class_of(int level)
{
    int n = 0;
    for (int i = 0; i < 16; i++) if (GS_CLASS_THRESHOLD[i] <= level + 1) n++;
    return n > 16 ? 16 : n;
}

static int first_set(uint32_t mask, const GsItem *list, int n)
{
    for (int i = 0; i < n; i++) if (mask >> list[i].bit & 1) return list[i].bit;
    return list[0].bit;
}

static bool is_hidden(int idx, const GsHidden *list, int n)
{
    for (int i = 0; i < n; i++) if (list[i].idx == idx) return true;
    return false;
}

void gs_fix_equipped(GsCard *c)
{
    uint32_t cm = gs_u32(c, GS_COSTUMES_MASK), wm = gs_u32(c, GS_WEAPONS_MASK);
    uint8_t *b = c->b + GS_SBHU;
    /* An item the locker cannot show has no bit to check, so it is left alone. */
    if (!(cm >> b[GS_EQ_COSTUME] & 1) && !is_hidden(b[GS_EQ_COSTUME], GS_HIDDEN_COSTUMES, GS_NHIDDEN_C)) {
        int bit = first_set(cm, GS_COSTUMES, GS_NCOSTUMES);
        b[GS_EQ_COSTUME] = (uint8_t)bit; b[GS_EQ_COSTUME2] = (uint8_t)bit;
    }
    if (!(wm >> b[GS_EQ_WEAPON] & 1) && !is_hidden(b[GS_EQ_WEAPON], GS_HIDDEN_WEAPONS, GS_NHIDDEN_W))
        b[GS_EQ_WEAPON] = (uint8_t)first_set(wm, GS_WEAPONS, GS_NWEAPONS);
}

bool gs_open(GsCard *c, const uint8_t *data, size_t n, char *err, size_t errlen)
{
    if (n != GS_CARD_SIZE) {
        snprintf(err, errlen, "This file is %zu bytes; a card is %d.", n, GS_CARD_SIZE);
        return false;
    }
    memcpy(c->b, data, GS_CARD_SIZE);
    if (memcmp(c->b + GS_SBHU, "SBHU", 4) != 0) {
        snprintf(err, errlen, "No save block found. This is not a Ghost Squad card.");
        return false;
    }
    if (memcmp(c->b + GS_SBHU + GS_GAME, "NXTG", 4) != 0) {
        snprintf(err, errlen, "This card belongs to another game.");
        return false;
    }
    uint32_t want = gs_crc32_jam(c->b + GS_SBHU, GS_CRC);
    c->crc_was_bad = gs_u32(c, GS_CRC) != want ||
                     memcmp(c->b + GS_MIRROR, c->b + GS_SBHU, 0x100) != 0;
    c->was_weapons  = gs_u32(c, GS_WEAPONS_MASK);
    c->was_costumes = gs_u32(c, GS_COSTUMES_MASK);
    c->was_features = gs_u32(c, GS_FEATURES);
    c->exp_edited = false;
    c->loaded = true;
    gs_fix_equipped(c);
    return true;
}

void gs_blank(GsCard *c)
{
    memset(c, 0, sizeof(*c));
    static const uint8_t factory[4] = { 0x95, 0x71, 0x36, 0x40 };
    memcpy(c->b + 0x20, factory, 4);
    c->b[0x36] = 0x03; c->b[0x37] = 0xF2;
    memcpy(c->b + GS_SBHU, "SBHU", 4);
    memcpy(c->b + GS_SBHU + GS_GAME, "NXTG", 4);
    gs_set_u32(c, GS_VERSION, 0x3F2);
    gs_set_u32(c, GS_LENGTH, 0x100);
    uint32_t now = (uint32_t)time(NULL);
    gs_set_u32(c, GS_CREATED, now);
    gs_set_u32(c, GS_WRITTEN, now);
    gs_set_u32(c, GS_AVAIL, 100);
    gs_set_u32(c, GS_WEAPONS_MASK, GS_VIRGIN_WEAPONS);     /* the XM-2119 */
    gs_set_u32(c, GS_COSTUMES_MASK, GS_VIRGIN_COSTUMES);   /* GHOST SQUAD and JUNGLE */
    for (int i = 0; i < GS_NMARKS; i++)
        c->b[GS_SBHU + GS_MARKS + i] = (uint8_t)(GS_VIRGIN_MARKS[i] - '0');
    c->b[GS_SBHU + GS_EQ_WEAPON] = 1;       /* the editor always shows something worn */
    c->b[GS_SBHU + 0x6D] = 0xFF;
    c->was_weapons = GS_VIRGIN_WEAPONS; c->was_costumes = GS_VIRGIN_COSTUMES; c->was_features = 0;
    c->loaded = true;
    gs_seal_for_save(c, false);
}

void gs_seal_for_save(GsCard *c, bool announce_as_new)
{
    gs_set_u32(c, GS_WRITTEN, (uint32_t)time(NULL));      /* the game stamps this at every write */
    /* The PLAYER'S DATA screen shows NEW when these differ from the live masks; the
     * game writes here what the card held when it went in, so we do the same. */
    gs_set_u32(c, GS_WAS_WEAPONS,  announce_as_new ? c->was_weapons  : gs_u32(c, GS_WEAPONS_MASK));
    gs_set_u32(c, GS_WAS_COSTUMES, announce_as_new ? c->was_costumes : gs_u32(c, GS_COSTUMES_MASK));
    gs_set_u32(c, GS_WAS_FEATURES, announce_as_new ? c->was_features : gs_u32(c, GS_FEATURES));
    gs_set_u32(c, GS_CRC, gs_crc32_jam(c->b + GS_SBHU, GS_CRC));
    memcpy(c->b + GS_MIRROR, c->b + GS_SBHU, 0x100);
}
