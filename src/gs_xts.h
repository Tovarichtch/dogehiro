/* Reading the game's own pictures out of a copy of the game the user owns.
 *
 * Nothing is known about the container: every sprite archive carries its own
 * shape (a zero dword, then the distance to its "XPR0" block), so the archives
 * are found by scanning for that. The card archive is the one holding every
 * texture the item tables name and a 30x20 rank badge at each of the seventeen
 * badge slots. Textures are DXT1 or DXT5, stored bottom-up.
 *
 * Copyright (c) 2026 Reda Cherif-Touil
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <vector>
#include <string>

struct GsTexture { int colour, w, h; size_t at; };
struct GsSprite  { int tex; int16_t rect[4]; };

struct GsArchive {
    std::vector<uint8_t>   data;
    std::vector<GsTexture> tex;
    std::vector<GsSprite>  spr;
    std::string            source;     /* file it came from */
    bool ok() const { return !tex.empty(); }
};

/* Scans the file and keeps the card archive. progress is called with 0..1. */
bool gs_find_archive(const char *path, GsArchive *out, std::string *err,
                     void (*progress)(float, void *), void *user);

/* The archive the game paints its HUD from, by name; only a real game image has it. */
bool gs_find_hud_archive(const char *path, GsArchive *out);

/* Cuts one sprite out of an archive. */
bool gs_cut_sprite(const GsArchive &a, int sprite, std::vector<uint8_t> &rgba, int *w, int *h);

/* Writes the name plates and icons of the items the locker cannot show. */
bool gs_export_hidden(const GsArchive &hud, const char *dir, std::string *err);

/* Decodes one texture to RGBA, rows already put back the right way up. */
bool gs_decode_texture(const GsArchive &a, int index, std::vector<uint8_t> &rgba, int *w, int *h);

/* The painted part of an item card: 88x115 inside a 128x128 square. */
void gs_painted_box(const std::vector<uint8_t> &rgba, int w, int h, int box[4]);

/* Writes costumes, weapons, badges and the two ITEM pictograms under dir. */
bool gs_export_assets(const GsArchive &a, const char *dir, bool japanese, std::string *err);

bool gs_write_png(const char *path, const uint8_t *rgba, int w, int h);
