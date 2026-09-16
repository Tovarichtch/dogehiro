/* Copyright (c) 2026 Reda Cherif-Touil
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* The parser must read both quoted strings and bare numbers. */
#include "gs_json.h"
#include <stdio.h>
#include <stdlib.h>
int main()
{
    struct { const char *req; int index; const char *want; } cases[] = {
        { "[976,1040]", 0, "976" },
        { "[976,1040]", 1, "1040" },
        { "[976.5, 1040]", 0, "976.5" },
        { "[\"world\",\"badges/05.png\"]", 1, "badges/05.png" },
        { "[\"card.bin\",\"QUJD\"]", 0, "card.bin" },
        { "[\"C:\\\\Users\\\\Reda\\\\a.bin\"]", 0, "C:\\Users\\Reda\\a.bin" },
        { "[1, \"two\", 3]", 2, "3" },
        { "[]", 0, "" },
    };
    int bad = 0;
    for (auto &c : cases) {
        const std::string got = gs_json_arg(c.req, c.index);
        const bool ok = got == c.want;
        printf("  %-32s [%d] -> %-24s %s\n", c.req, c.index, ("\"" + got + "\"").c_str(), ok ? "OK" : "ECHEC");
        bad += !ok;
    }
    printf("%s\n", bad ? "DES CAS ECHOUENT" : "l'analyseur lit nombres et chaines");
    return bad ? 1 : 0;
}
