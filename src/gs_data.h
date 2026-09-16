/* Ghost Squad card tables, taken from the game itself (vsg.xbe).
 * Item bits and names, where each picture sits in the sprite archive, EXP
 * milestones, class thresholds and the rank badge sprites. Facts about the
 * save format, not artwork.
 *
 * Copyright (c) 2026 Reda Cherif-Touil
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once
#include <stdint.h>

struct GsItem { int bit, tex_jp, tex_world, tex_locked; const char *name; };

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
/* Items the locker cannot show: the game has a model for them but no locker bit,
   no item card and no picture. The setters accept costume 0-16 (FUN_0009AC50) and
   weapon 1-36 (FUN_0009ABF0), while the locker lists only 14 and 25. Names are the
   game's own, from the weapon table at 0x244000. They can only be worn. */
struct GsHidden { int idx; const char *name; int plate, icon; };
/* plate and icon are sprite numbers in spr_anime_game.xts, the archive the game
   paints its HUD from; the weapon's own record (0x244E78 + i*0x24) names them.
   The six grenades carry 0xFFFF there: they have no plate at all. */
static const GsHidden GS_HIDDEN_WEAPONS[] = {
    { 26, "SAN92",             352, 353 }, { 27, "CAC80",    322,  16 },
    { 28, "CAC80 II",          322,  16 }, { 29, "SAW25",    354, 355 },
    { 30, "GRENADE",            -1,  -1 }, { 31, "GGL50",    350, 351 },
    { 32, "SMOKE GRENADE",      -1,  -1 }, { 33, "CS GRENADE",     -1, -1 },
    { 34, "FLASH GRENADE",      -1,  -1 }, { 35, "ROAR GRENADE",   -1, -1 },
    { 36, "STINGBALL GRENADE",  -1,  -1 },
};
static const GsHidden GS_HIDDEN_COSTUMES[] = {
    { 1, "BETA", -1, -1 }, { 2, "GAMMA", -1, -1 }, { 16, "FORCE X", -1, -1 },
};
#define GS_NHIDDEN_W ((int)(sizeof(GS_HIDDEN_WEAPONS)/sizeof(GS_HIDDEN_WEAPONS[0])))
#define GS_NHIDDEN_C ((int)(sizeof(GS_HIDDEN_COSTUMES)/sizeof(GS_HIDDEN_COSTUMES[0])))

/* What the game's own builder puts on a brand new card (FUN_0002b740):
   two costumes - GHOST SQUAD and JUNGLE - one weapon, and the 56 progress
   markers laid down by FUN_00099a90 (a 1 wherever the condition table at
   0x2B63C0/D4/E8 holds a 1). The three groups are 17, 19 and 20 items long. */
#define GS_VIRGIN_WEAPONS  0x02
#define GS_VIRGIN_COSTUMES 0x11
static const char GS_VIRGIN_MARKS[] = "11101100111110011"
                                      "0101110000010001111"
                                      "11111010111111000011";

#define GS_NCOSTUMES ((int)(sizeof(GS_COSTUMES)/sizeof(GS_COSTUMES[0])))
#define GS_NWEAPONS  ((int)(sizeof(GS_WEAPONS)/sizeof(GS_WEAPONS[0])))

/* milestone[n] = sum of round(curve[i] * 3500) for i <= n; curve at vsg.xbe 0x2DF290 */
static const uint32_t GS_MILESTONE[100] = {
    0, 190, 717, 1552, 2605, 3828, 5190, 6669, 8249, 9919,
    11669, 13491, 15380, 17329, 19335, 21393, 23500, 25653, 27850, 30088,
    32365, 34679, 37028, 39411, 41826, 44273, 46749, 49254, 51787, 54346,
    56931, 59541, 62175, 64833, 67513, 70215, 72939, 75683, 78448, 81232,
    84035, 86857, 89698, 92556, 95432, 98325, 101235, 104161, 107103, 110061,
    113034, 116022, 119025, 122042, 125074, 128120, 131179, 134252, 137338, 140437,
    143549, 146673, 149810, 152959, 156120, 159293, 162477, 165672, 168879, 172097,
    175326, 178566, 181816, 185077, 188348, 191629, 194920, 198221, 201532, 204853,
    208183, 211523, 214872, 218230, 221597, 224973, 228358, 231752, 235155, 238566,
    241986, 245414, 248851, 252296, 255749, 259210, 262679, 266156, 269641, 273141,
};
/* a class is reached when its threshold is <= level + 1 (table at 0x231A8A) */
static const int GS_CLASS_THRESHOLD[16] = {
    4, 7, 10, 13, 15, 18, 20, 23, 27, 30, 34, 38, 43, 50, 60, 99,
};
static const char *GS_CLASS_NAME[17] = {
    "Private", "Private First Class", "Sergeant", "Master Sergeant", "Warrant Officer",
    "Chief Warrant Officer", "Second Lieutenant", "First Lieutenant", "Captain", "Major",
    "Lieutenant Colonel", "Colonel", "Brigadier General", "Major General",
    "Lieutenant General", "General", "Marshal"
};
static const char *GS_CLASS_ABBR[17] = {
    "PVT","PFC","SGT","MSG","WO","CW","2LT","1LT","CPT","MAJ","LTC","COL","BG","MG","LTG","GEN","MAR"
};
/* rank badge sprite per class (badge table at 0x20DF48) */
static const int GS_BADGE_SPRITE[17] = { 165,164,163,162,161,160,159,58,57,56,55,54,53,52,51,50,49 };
/* the two green pictograms live inside the ITEM panel sprite */
/* the button plates the game draws its menu choices on */
#define GS_PLATE_SPRITE 335
#define GS_PLATE_ON_SPRITE 336

#define GS_ICON_SPRITE 338
static const int GS_ICON_GUN[4]   = { 48, 37, 78,  67 };
static const int GS_ICON_SHIRT[4] = { 48, 77, 78, 107 };
