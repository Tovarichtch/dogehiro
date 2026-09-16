/* Checks the C++ core against the Python reference: same card bytes, same pictures.
 * Copyright (c) 2026 Reda Cherif-Touil
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "gs_card.h"
#include "gs_data.h"
#include "gs_xts.h"
#include <stdio.h>
#include <string.h>
#include <vector>
#include <string>

static int fails = 0;
static void check(bool ok, const char *what, const char *detail = "")
{
    printf("  %-52s %s%s\n", what, ok ? "OK" : "ECHEC", detail);
    if (!ok) fails++;
}

static std::vector<uint8_t> slurp(const char *p)
{
    std::vector<uint8_t> v;
    FILE *f = fopen(p, "rb");
    if (!f) return v;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    v.resize((size_t)n);
    if (fread(v.data(), 1, v.size(), f) != v.size()) v.clear();
    fclose(f);
    return v;
}

int main(int argc, char **argv)
{
    const char *card_path = argc > 1 ? argv[1] : "/home/reda/Documents/reda_card.bin";
    const char *game_path = argc > 2 ? argv[2] : "/home/reda/Downloads/chihiro-netboot/ghostsqu.bin";

    printf("Carte\n");
    std::vector<uint8_t> raw = slurp(card_path);
    GsCard c{};
    char err[256] = "";
    check(gs_open(&c, raw.data(), raw.size(), err, sizeof err), "la carte s'ouvre", err);
    check(!c.crc_was_bad, "somme de controle et miroir intacts");

    uint32_t exp = gs_u32(&c, GS_EXP);
    int lvl = gs_level_of(exp), cls = gs_class_of(lvl);
    char d[128];
    snprintf(d, sizeof d, "   (EXP %u -> niveau %d, classe %d)", exp, lvl, cls);
    check(lvl == c.b[GS_SBHU + GS_LEVEL], "niveau calcule = niveau stocke", d);
    check(cls == c.b[GS_SBHU + GS_CLASS], "classe calculee = classe stockee");

    /* resceller sans rien changer ne doit toucher que la date et les sommes */
    std::vector<uint8_t> before(c.b, c.b + GS_CARD_SIZE);
    gs_seal_for_save(&c, true);
    int moved = 0, unexpected = 0;
    for (int i = 0; i < GS_CARD_SIZE; i++)
        if (before[i] != c.b[i]) {
            moved++;
            const int o = i - GS_SBHU, m = i - GS_MIRROR;
            bool ok = (o >= GS_WRITTEN && o < GS_WRITTEN + 4) || (o >= GS_CRC && o < GS_CRC + 4) ||
                      (m >= GS_WRITTEN && m < GS_WRITTEN + 4) || (m >= GS_CRC && m < GS_CRC + 4);
            if (!ok) unexpected++;
        }
    snprintf(d, sizeof d, "   (%d octets, %d inattendus)", moved, unexpected);
    check(unexpected == 0, "sauvegarde a vide : date et sommes seulement", d);
    check(gs_u32(&c, GS_CRC) == gs_crc32_jam(c.b + GS_SBHU, GS_CRC), "somme recalculee valide");
    check(memcmp(c.b + GS_MIRROR, c.b + GS_SBHU, 0x100) == 0, "miroir identique au bloc principal");

    /* verrouiller le costume porte doit rehabiller le personnage */
    uint32_t cm = gs_u32(&c, GS_COSTUMES_MASK);
    int worn = c.b[GS_SBHU + GS_EQ_COSTUME];
    gs_set_u32(&c, GS_COSTUMES_MASK, cm & ~(1u << worn));
    gs_fix_equipped(&c);
    int now = c.b[GS_SBHU + GS_EQ_COSTUME];
    check((gs_u32(&c, GS_COSTUMES_MASK) >> now & 1) != 0, "l'objet porte est toujours debloque");
    gs_set_u32(&c, GS_COSTUMES_MASK, cm);

    {   /* an item the locker cannot show must survive the equipped-item repair */
        GsCard h = c;
        h.b[GS_SBHU + GS_EQ_WEAPON] = 36; h.b[GS_SBHU + GS_EQ_COSTUME] = 16;
        gs_fix_equipped(&h);
        check(h.b[GS_SBHU + GS_EQ_WEAPON] == 36 && h.b[GS_SBHU + GS_EQ_COSTUME] == 16,
              "un objet hors casier reste porte", " (arme 36, costume 16)");
    }

    printf("Carte vierge\n");
    GsCard blank{};
    gs_blank(&blank);
    check(memcmp(blank.b + GS_SBHU, "SBHU", 4) == 0 && memcmp(blank.b + GS_SBHU + GS_GAME, "NXTG", 4) == 0,
          "signatures posees");
    check(gs_u32(&blank, GS_CRC) == gs_crc32_jam(blank.b + GS_SBHU, GS_CRC), "carte vierge scellee");
    check(gs_u32(&blank, GS_AVAIL) == 100 && gs_u32(&blank, GS_WEAPONS_MASK) == GS_VIRGIN_WEAPONS &&
          gs_u32(&blank, GS_COSTUMES_MASK) == GS_VIRGIN_COSTUMES,
          "100 parties, une arme et DEUX costumes", " (0x11 = GHOST SQUAD + JUNGLE)");
    bool marks_ok = true;
    for (int i = 0; i < GS_NMARKS; i++)
        if (blank.b[GS_SBHU + GS_MARKS + i] != (uint8_t)(GS_VIRGIN_MARKS[i] - '0')) marks_ok = false;
    check(marks_ok && strlen(GS_VIRGIN_MARKS) == GS_NMARKS, "les 56 marqueurs comme le jeu les pose");

    printf("Images du jeu\n");
    GsArchive a;
    std::string e;
    if (!gs_find_archive(game_path, &a, &e, NULL, NULL)) {
        check(false, "archive trouvee dans l'image du jeu", e.c_str());
        return fails ? 1 : 0;
    }
    snprintf(d, sizeof d, "   (%zu textures, %zu sprites)", a.tex.size(), a.spr.size());
    check(a.tex.size() == 329 && a.spr.size() == 398, "archive des cartes reconnue", d);

    std::vector<uint8_t> px; int w, h, box[4];
    check(gs_decode_texture(a, 0, px, &w, &h) && w == 128 && h == 128, "texture 0 decodee");
    gs_painted_box(px, w, h, box);
    snprintf(d, sizeof d, "   (%d,%d)-(%d,%d)", box[0], box[1], box[2], box[3]);
    check(box[0] == 20 && box[1] == 6 && box[2] == 108 && box[3] == 121, "zone peinte = 88x115 a (20,6)", d);

    /* meme pixels que l'extraction Python */
    /* Optional: raw RGBA of textures 0 and 318 as the reference tool decodes them,
     * to prove the C++ decoder agrees with it. Skipped when not given. */
    std::vector<uint8_t> ref = argc > 3 ? slurp(argv[3]) : std::vector<uint8_t>();
    if (ref.size() >= 65536) {
        check(memcmp(px.data(), ref.data(), 65536) == 0, "texture 0 identique au decodeur Python");
        std::vector<uint8_t> px2;
        if (gs_decode_texture(a, 318, px2, &w, &h) && ref.size() >= 131072 + 262144)
            check(memcmp(px2.data(), ref.data() + 131072, 262144) == 0, "texture 318 (DXT1) identique");
    }
    for (int i = 0; i < 17; i++) {
        const GsSprite &s = a.spr[GS_BADGE_SPRITE[i]];
        if (s.rect[2] - s.rect[0] != 30 || s.rect[3] - s.rect[1] != 20) { check(false, "insignes 30x20"); break; }
        if (i == 16) check(true, "les 17 insignes font 30x20");
    }
    printf("%s\n", fails ? "DES ESSAIS ONT ECHOUE" : "tout est bon");
    return fails ? 1 : 0;
}
