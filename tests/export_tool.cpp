/* Writes the asset folders with the same code the application uses.
 * Copyright (c) 2026 Reda Cherif-Touil
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "gs_xts.h"
#include <stdio.h>
int main(int argc, char **argv)
{
    if (argc < 3) { printf("usage: export_tool <game image> <assets dir>\n"); return 2; }
    GsArchive a; std::string err;
    if (!gs_find_archive(argv[1], &a, &err, NULL, NULL)) { printf("%s\n", err.c_str()); return 1; }
    printf("archive: %zu textures, %zu sprites\n", a.tex.size(), a.spr.size());
    GsArchive hud;
    const bool has_hud = gs_find_hud_archive(argv[1], &hud);
    printf("bandeau: %s\n", has_hud ? "trouve" : "absent");
    for (int jp = 0; jp < 2; jp++) {
        std::string dir = std::string(argv[2]) + (jp ? "/japanese" : "/world");
        if (!gs_export_assets(a, dir.c_str(), jp != 0, &err)) { printf("%s\n", err.c_str()); return 1; }
        if (has_hud && !gs_export_hidden(hud, dir.c_str(), &err)) { printf("%s\n", err.c_str()); return 1; }
        printf("ecrit %s\n", dir.c_str());
    }
    return 0;
}
