/* A Ghost Squad magnetic card: the 2048-byte image the reader gives the game.
 *
 * The game keeps one 256-byte save block at card offset 0x40 and an identical
 * copy at 0x140; it accepts the card only when the game id, the version, the
 * length and the CRC-32/JAMCRC over the first 252 bytes all agree. No field is
 * range-checked, so a sealed card is always accepted.
 *
 * Copyright (c) 2026 Reda Cherif-Touil
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

#define GS_CARD_SIZE 2048
#define GS_SBHU      0x40
#define GS_MIRROR    0x140

/* offsets inside the save block */
enum {
    GS_VERSION = 0x04, GS_GAME = 0x08, GS_LENGTH = 0x0C,
    GS_CREATED = 0x10, GS_WRITTEN = 0x14, GS_UID = 0x20,
    GS_AVAIL = 0x24, GS_COUNT = 0x28, GS_STATE = 0x2C, GS_NAME = 0x30,
    GS_BEST = 0x44, GS_PLAY = 0x48, GS_EXP = 0x50, GS_LEVEL = 0x5C, GS_CLASS = 0x64,
    GS_WEAPONS_MASK = 0x74, GS_COSTUMES_MASK = 0x78,
    GS_EQ_WEAPON = 0x7C, GS_EQ_COSTUME = 0x7D, GS_EQ_COSTUME2 = 0x7E,
    GS_MARKS = 0x80, GS_NMARKS = 56,
    GS_FEATURES = 0xB8, GS_TOTAL = 0xC4, GS_CONFIG = 0xCC,
    GS_WAS_WEAPONS = 0xD0, GS_WAS_COSTUMES = 0xD4, GS_WAS_FEATURES = 0xD8,
    GS_CRC = 0xFC
};

struct GsCard {
    uint8_t  b[GS_CARD_SIZE];
    uint32_t was_weapons, was_costumes, was_features;  /* the masks as the file was opened */
    bool     loaded;
    bool     exp_edited;      /* level and class are rewritten only once the player moves EXP */
    bool     crc_was_bad;
};

uint32_t gs_crc32_jam(const uint8_t *p, size_t n);
uint32_t gs_u32(const GsCard *c, int off);
void     gs_set_u32(GsCard *c, int off, uint32_t v);

/* Returns false and fills err when the file is not a Ghost Squad card. */
bool gs_open(GsCard *c, const uint8_t *data, size_t n, char *err, size_t errlen);
void gs_blank(GsCard *c);
/* Stamps the write time, writes what the card looked like on the way in, seals both slots. */
void gs_seal_for_save(GsCard *c, bool announce_as_new);

int  gs_level_of(uint32_t exp);     /* stored level 0..98; the game shows this plus one */
int  gs_class_of(int level);
void gs_fix_equipped(GsCard *c);    /* the game never wears an item that is not unlocked */
