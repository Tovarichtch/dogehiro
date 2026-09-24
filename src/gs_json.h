/* Reads one argument of the JSON array the page sends: a quoted string or a bare number.
 *
 * Copyright (c) 2026 Reda Cherif-Touil
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once
#include <string>

inline std::string gs_json_arg(const std::string &req, int index)
{
    int seen = -1;
    size_t i = 0;
    while (i < req.size()) {
        const char c = req[i];
        if (c == '[' || c == ']' || c == ',' || c == ' ' || c == '\t' || c == '\n' || c == '\r') { ++i; continue; }
        std::string v;
        if (c == '"') {
            for (++i; i < req.size() && req[i] != '"'; ++i) {
                if (req[i] == '\\' && i + 1 < req.size()) {
                    ++i;
                    v += req[i] == 'n' ? '\n' : req[i] == 't' ? '\t' : req[i];
                } else v += req[i];
            }
            if (i < req.size()) ++i;                      /* closing quote */
        } else {
            while (i < req.size() && req[i] != ',' && req[i] != ']') v += req[i++];
        }
        if (++seen == index) return v;
    }
    return std::string();
}
