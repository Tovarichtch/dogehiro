/* Where the importer finds each picture in the game's archives (read from vsg.xbe).
 *
 * Copyright (c) 2026 Reda Cherif-Touil
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once
#include <stdint.h>

struct GsItem { int bit, tex_jp, tex_world, tex_locked; const char *name; };   /* tex_*: card textures */

static const GsItem GS_COSTUMES[] = {
    {  0,   0,   0,   1, "GHOST SQUAD" },
    {  4,   2,   2,   3, "JUNGLE" },
    {  3,   4,   4,   5, "DESERT" },
    {  5,   8,   8,   9, "TOUGH GUY" },
    {  6,   6,   6,   7, "POLICE MAN" },
    {  7,  12,  12,  13, "WW2" },
    {  8,  16,  16,  17, "URBAN" },
    {  9,  14,  14,  15, "COWBOY" },
    { 10,  18,  18,  19, "VIRTUA COP" },
    { 11,  10,  10,  11, "SKY" },
    { 12,  22,  22,  23, "NINJA" },
    { 13,  24,  24,  25, "PANDA" },
    { 14,  20,  20,  21, "FUTURE WARRIOR" },
    { 15,  26,  26,  27, "GOLD" },
};
static const GsItem GS_WEAPONS[] = {
    {  1,  56,  56,  57, "XM-2119" },
    {  2,  58,  58,  59, "M94R" },
    {  3,  60,  60,  61, "MP10" },
    {  6,  62,  62,  63, "AR4A" },
    {  4,  64,  65,  66, "TK1B" },
    {  7,  67,  68,  69, "TR14" },
    {  8,  70,  70,  71, "G50" },
    {  5,  72,  73,  74, "CPG7" },
    {  9,  75,  76,  77, "SAW24" },
    { 10,  78,  79,  80, "XMW21" },
    { 11,  81,  81,  82, "M209" },
    { 12,  83,  84,  85, "TM9V" },
    { 13,  86,  87,  88, "PM6" },
    { 14,  89,  89,  90, "P4512" },
    { 15,  91,  92,  93, "P44M" },
    { 16,  94,  94,  95, "M45R" },
    { 17,  96,  96,  97, "GUARDIAN II" },
    { 18,  98,  99, 100, "GUARDIAN I" },
    { 23, 101, 101, 102, "M4E" },
    { 24, 103, 103, 104, "SG112" },
    { 25, 105, 106, 107, "ABG1" },
    { 19, 108, 109, 110, "CAC82" },
    { 20, 111, 112, 113, "SPR3" },
    { 21, 114, 115, 116, "SR308" },
    { 22, 117, 118, 119, "SPR11" },
};
/* Weapons with a model but no locker entry (weapon table at 0x244000). */
struct GsHidden { int idx; const char *name; int plate, icon; };   /* HUD sprites, -1 = none */
static const GsHidden GS_HIDDEN_WEAPONS[] = {
    { 26, "SAN92",             352, 353 }, { 27, "CAC80",    322,  16 },
    { 28, "CAC80 II",          322,  16 }, { 29, "SAW25",    354, 355 },
    { 30, "GRENADE",            -1,  -1 }, { 31, "GGL50",    350, 351 },
    { 32, "SMOKE GRENADE",      -1,  -1 }, { 33, "CS GRENADE",     -1, -1 },
    { 34, "FLASH GRENADE",      -1,  -1 }, { 35, "ROAR GRENADE",   -1, -1 },
    { 36, "STINGBALL GRENADE",  -1,  -1 },
};
#define GS_NHIDDEN_W ((int)(sizeof(GS_HIDDEN_WEAPONS)/sizeof(GS_HIDDEN_WEAPONS[0])))

#define GS_NCOSTUMES ((int)(sizeof(GS_COSTUMES)/sizeof(GS_COSTUMES[0])))
#define GS_NWEAPONS  ((int)(sizeof(GS_WEAPONS)/sizeof(GS_WEAPONS[0])))

/* rank badge sprite per class (badge table at 0x20DF48) */
static const int GS_BADGE_SPRITE[17] = { 165,164,163,162,161,160,159,58,57,56,55,54,53,52,51,50,49 };
/* the button plate the game draws its menu choices on */
#define GS_PLATE_SPRITE 335
/* the ITEM panel's two pictograms */
#define GS_ICON_SPRITE 338
static const int GS_ICON_GUN[4]   = { 48, 37, 78,  67 };
static const int GS_ICON_SHIRT[4] = { 48, 77, 78, 107 };
