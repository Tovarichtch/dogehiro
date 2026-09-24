/* Open and save dialogs: Windows' own, osascript on macOS, zenity or kdialog on Linux.
 *
 * Copyright (c) 2026 Reda Cherif-Touil
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "gs_dialog.h"
#include <stdio.h>
#include <string.h>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>

static bool win_dialog(bool save, const char *title, const char *filter,
                       const char *suggested, std::string &out)
{
    char buf[MAX_PATH] = "";
    if (suggested) snprintf(buf, sizeof buf, "%s", suggested);
    OPENFILENAMEA o = {};
    o.lStructSize = sizeof o;
    o.lpstrFilter = filter;
    o.lpstrFile   = buf;
    o.nMaxFile    = sizeof buf;
    o.lpstrTitle  = title;
    o.lpstrDefExt = save ? "bin" : NULL;
    o.Flags = save ? (OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR)
                   : (OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR);
    if (!(save ? GetSaveFileNameA(&o) : GetOpenFileNameA(&o))) return false;
    out = buf;
    return true;
}

bool gs_dialog_open(const char *title, const char *filter, std::string &out)
{ return win_dialog(false, title, filter, NULL, out); }
bool gs_dialog_save(const char *title, const char *suggested, std::string &out)
{ return win_dialog(true, title, "Card image (*.bin)\0*.bin\0All files\0*.*\0", suggested, out); }
bool gs_dialog_available() { return true; }

#else  /* macOS and Linux */

static bool run(const std::string &cmd, std::string &out)
{
    FILE *p = popen(cmd.c_str(), "r");
    if (!p) return false;
    char buf[4096];
    out.clear();
    while (fgets(buf, sizeof buf, p)) out += buf;
    int rc = pclose(p);
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
    return rc == 0 && !out.empty();
}

static bool have(const char *tool)
{
    std::string sink;
    return run(std::string("command -v ") + tool + " 2>/dev/null", sink);
}

bool gs_dialog_available()
{
#ifdef __APPLE__
    return true;
#else
    return have("zenity") || have("kdialog");
#endif
}

bool gs_dialog_open(const char *title, const char *, std::string &out)
{
#ifdef __APPLE__
    return run(std::string("osascript -e 'POSIX path of (choose file with prompt \"") + title + "\")' 2>/dev/null", out);
#else
    if (have("zenity"))
        return run(std::string("zenity --file-selection --title=\"") + title + "\" 2>/dev/null", out);
    if (have("kdialog"))
        return run(std::string("kdialog --getopenfilename . --title \"") + title + "\" 2>/dev/null", out);
    return false;
#endif
}

bool gs_dialog_save(const char *title, const char *suggested, std::string &out)
{
#ifdef __APPLE__
    /* "default name" takes a name only; the folder goes to "default location" */
    std::string name = suggested ? suggested : "card.bin", where;
    const size_t slash = name.rfind('/');
    if (slash != std::string::npos) {
        where = " default location (POSIX file \"" + (slash ? name.substr(0, slash) : "/") + "\")";
        name = name.substr(slash + 1);
    }
    if (name.empty()) name = "card.bin";
    return run(std::string("osascript -e 'POSIX path of (choose file name with prompt \"") + title +
               "\" default name \"" + name + "\"" + where + ")' 2>/dev/null", out);
#else
    if (have("zenity"))
        return run(std::string("zenity --file-selection --save --confirm-overwrite --title=\"") + title +
                   "\" --filename=\"" + (suggested ? suggested : "card.bin") + "\" 2>/dev/null", out);
    if (have("kdialog"))
        return run(std::string("kdialog --getsavefilename \"") + (suggested ? suggested : "card.bin") +
                   "\" --title \"" + title + "\" 2>/dev/null", out);
    return false;
#endif
}
#endif
