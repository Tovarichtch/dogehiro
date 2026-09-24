/* Dogehiro - a Ghost Squad card editor.
 * The window shows ui/index.html; this file opens and saves cards for it and
 * extracts the game's pictures.
 *
 * Copyright (c) 2026 Reda Cherif-Touil
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#define WEBVIEW_STATIC
#define WEBVIEW_IMPLEMENTATION
#include "webview.h"

#include "ui_embed.h"
#include "gs_dialog.h"
#include "gs_xts.h"
#include "gs_json.h"
#include "gs_icon.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#ifdef __linux__
#include <gtk/gtk.h>
#endif
#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <sys/stat.h>
#endif

/* ------------------------------------------------------------ small helpers */

static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string b64_encode(const uint8_t *p, size_t n)
{
    std::string o;
    o.reserve((n + 2) / 3 * 4);
    for (size_t i = 0; i < n; i += 3) {
        const uint32_t v = (uint32_t)p[i] << 16 | (i + 1 < n ? (uint32_t)p[i + 1] << 8 : 0)
                                                | (i + 2 < n ? (uint32_t)p[i + 2] : 0);
        o += B64[(v >> 18) & 63];
        o += B64[(v >> 12) & 63];
        o += i + 1 < n ? B64[(v >> 6) & 63] : '=';
        o += i + 2 < n ? B64[v & 63] : '=';
    }
    return o;
}

static bool b64_decode(const std::string &s, std::vector<uint8_t> &out)
{
    int8_t rev[256];
    memset(rev, -1, sizeof rev);
    for (int i = 0; i < 64; i++) rev[(unsigned char)B64[i]] = (int8_t)i;
    uint32_t acc = 0;
    int bits = 0;
    out.clear();
    for (char ch : s) {
        if (ch == '=' || ch == '\n' || ch == '\r') continue;
        const int8_t v = rev[(unsigned char)ch];
        if (v < 0) return false;
        acc = (acc << 6) | (uint32_t)v;
        bits += 6;
        if (bits >= 8) { bits -= 8; out.push_back((uint8_t)((acc >> bits) & 0xFF)); }
    }
    return true;
}

static std::string json_quote(const std::string &s)
{
    std::string o = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += c; }
        else if ((unsigned char)c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); o += b; }
        else o += c;
    }
    return o + "\"";
}

static std::string base_name(const std::string &p)
{
    const size_t s = p.find_last_of("/\\");
    return s == std::string::npos ? p : p.substr(s + 1);
}

static std::string fail(const std::string &why) { return "{\"ok\":false,\"err\":" + json_quote(why) + "}"; }

/* ------------------------------------------------------------------- state */

static GsArchive  g_game;          /* card pictures, from the player's game image */
static GsArchive  g_hud;           /* HUD pictures, when the image has them */
static std::string g_assets;       /* the assets folder */
static std::string g_card_path;    /* the card being edited */
static float       g_screen = 1.0f;     /* screen pixels per page pixel (Windows) */

static bool folder_has_art(const std::string &lang)
{
    const std::string probe = g_assets + "/" + lang + "/costumes/00-ghost-squad.png";
    FILE *f = fopen(probe.c_str(), "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

static std::string exe_dir()
{
#ifdef _WIN32
    char buf[MAX_PATH];
    const DWORD n = GetModuleFileNameA(NULL, buf, sizeof buf);
    std::string p(buf, n);
#elif defined(__APPLE__)
    char buf[4096];
    uint32_t n = sizeof buf;                       /* macOS has no /proc */
    std::string p(_NSGetExecutablePath(buf, &n) == 0 ? buf : "./dogehiro");
#else
    char buf[4096];
    const ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 1);
    std::string p(buf, n > 0 ? (size_t)n : 0);
    if (p.empty()) p = "./dogehiro";
#endif
    const size_t s = p.find_last_of("/\\");
    return s == std::string::npos ? std::string(".") : p.substr(0, s);
}

/* Where pictures and settings go: next to the program, or in Application Support
 * on macOS, where the bundle may be read-only. */
static std::string data_dir()
{
#ifdef __APPLE__
    const char *home = getenv("HOME");
    const std::string d = std::string(home && *home ? home : ".") + "/Library/Application Support/Dogehiro";
    mkdir(d.c_str(), 0755);
    return d;
#else
    return exe_dir();
#endif
}

static std::string pref_path() { return data_dir() + "/dogehiro.cfg"; }   /* the page size chosen */

#ifdef __linux__
/* Room left for the page (screen minus title bar and margin) and the window's size. */
static bool linux_desk(void *win, int margin, int *dw, int *dh, int *cw, int *ch)
{
    GdkDisplay *dpy = gdk_display_get_default();
    GdkMonitor *mon = dpy ? gdk_display_get_primary_monitor(dpy) : NULL;
    if (!mon && dpy && gdk_display_get_n_monitors(dpy) > 0) mon = gdk_display_get_monitor(dpy, 0);
    if (!mon) return false;
    GdkRectangle area;
    gdk_monitor_get_workarea(mon, &area);
    int frame_w = 0, frame_h = 40;                  /* until the title bar is measured */
    gtk_window_get_size(GTK_WINDOW(win), cw, ch);
    if (GdkWindow *gw = gtk_widget_get_window(GTK_WIDGET(win))) {
        GdkRectangle ext;
        gdk_window_get_frame_extents(gw, &ext);
        if (ext.height > *ch && ext.height - *ch < 200) { frame_h = ext.height - *ch; frame_w = ext.width - *cw; }
    }
    *dw = area.width - frame_w - margin;
    *dh = area.height - frame_h - margin;
    return true;
}
#endif

/* --------------------------------------------------------------------- main */

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR cmdline, int)
#else
int main(int argc, char **argv)
#endif
{
    /* A card named on the command line opens at startup. */
#ifdef _WIN32
    if (cmdline && *cmdline) {
        std::string a = cmdline;
        if (a.size() > 1 && a.front() == '"' && a.back() == '"') a = a.substr(1, a.size() - 2);
        g_card_path = a;
    }
#else
    if (argc > 1 && argv[1][0] != '-') g_card_path = argv[1];
#endif
#ifdef __linux__
    g_set_prgname("dogehiro");                   /* Wayland finds the icon by this name */
    gdk_set_program_class("Dogehiro");
    /* Some drivers refuse WebKit's GPU buffers ("Failed to create GBM buffer"):
     * blank window. The page draws fine without them. */
    if (!getenv("WEBKIT_DISABLE_DMABUF_RENDERER"))
        setenv("WEBKIT_DISABLE_DMABUF_RENDERER", "1", 1);
#endif
    g_assets = data_dir() + "/assets";

    try {
        webview::webview w(false, nullptr);
        w.set_title("Dogehiro - Ghost Squad card editor for Sega Chihiro");

        float &screen = g_screen;                        /* Windows display scaling */
#ifdef _WIN32
        if (HMODULE user32 = LoadLibraryA("user32.dll")) {
            typedef UINT(WINAPI * dpi_fn)(void);
            if (dpi_fn f = (dpi_fn)GetProcAddress(user32, "GetDpiForSystem")) {
                const UINT dpi = f();
                if (dpi >= 96) screen = (float)dpi / 96.0f;
            }
        }
#endif
        int want_w = 990, want_h = 880;                  /* page pixels */
#ifdef _WIN32
        RECT work{};                                     /* fit the desktop */
        if (SystemParametersInfoA(SPI_GETWORKAREA, 0, &work, 0)) {
            const int mw = (int)((work.right - work.left - 40) / screen);
            const int mh = (int)((work.bottom - work.top - 40) / screen);
            if (want_w > mw) want_w = mw;
            if (want_h > mh) want_h = mh;
        }
#endif
        w.set_size(want_w, want_h, WEBVIEW_HINT_NONE);
        w.set_size(360, 180, WEBVIEW_HINT_MIN);   /* free to shrink to the page */

#ifdef _WIN32
        {   /* window and task bar icon */
            HWND hwnd = (HWND)w.window().value();
            HINSTANCE self = GetModuleHandleA(NULL);
            if (HICON big = (HICON)LoadImageA(self, MAKEINTRESOURCEA(1), IMAGE_ICON,
                                              GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0))
                SendMessageA(hwnd, WM_SETICON, ICON_BIG, (LPARAM)big);
            if (HICON small_icon = (HICON)LoadImageA(self, MAKEINTRESOURCEA(1), IMAGE_ICON,
                                                     GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0))
                SendMessageA(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)small_icon);
        }
#endif

#ifdef __linux__
        if (void *win = w.window().value()) {          /* window icon (X11) */
            GdkPixbufLoader *ld = gdk_pixbuf_loader_new();
            if (gdk_pixbuf_loader_write(ld, GS_ICON_PNG, sizeof GS_ICON_PNG, NULL) &&
                gdk_pixbuf_loader_close(ld, NULL)) {
                if (GdkPixbuf *pix = gdk_pixbuf_loader_get_pixbuf(ld))
                    gtk_window_set_icon(GTK_WINDOW(win), pix);
            }
            g_object_unref(ld);
        }
#endif

        /* Window scale, room on screen and window size, for the page's zoom. */
        w.bind("hostScale", [&w](const std::string &) -> std::string {
            double ws = 1.0;
            int sw = 0, sh = 0;                 /* room on screen, in window units */
            int cw = 0, ch = 0;                 /* window size, same units */
#ifdef _WIN32
            ws = g_screen;
            RECT work{};                        /* in page pixels, like set_size */
            if (SystemParametersInfoA(SPI_GETWORKAREA, 0, &work, 0)) {
                sw = (int)((work.right - work.left) / g_screen) - 16;
                sh = (int)((work.bottom - work.top) / g_screen) - 48;
            }
            RECT cr{};
            if (GetClientRect((HWND)w.window().value(), &cr)) {
                cw = (int)((cr.right - cr.left) / g_screen);
                ch = (int)((cr.bottom - cr.top) / g_screen);
            }
#elif defined(__linux__)
            /* a margin: under Wayland a window cannot move up, so a full-height one loses its bottom */
            if (void *win = w.window().value()) {
                ws = gtk_widget_get_scale_factor(GTK_WIDGET(win));
                linux_desk(win, 24, &sw, &sh, &cw, &ch);
            }
#endif
            char out[192];
            snprintf(out, sizeof out, "{\"scale\":%.4f,\"w\":%d,\"h\":%d,\"cw\":%d,\"ch\":%d}",
                     ws, sw, sh, cw, ch);
            return out;
        });

        /* The page asks for a window size; the answer is what the screen allows. */
        w.bind("hostResize", [&w](const std::string &req) -> std::string {
            const double want_w = atof(gs_json_arg(req, 0).c_str());
            const double want_h = atof(gs_json_arg(req, 1).c_str());
            if (want_w < 100 || want_h < 60) return "{\"w\":0,\"h\":0}";
            int pw = (int)(want_w + 0.5), ph = (int)(want_h + 0.5);   /* page pixels: set_size scales them */
            int limit_w = 100000, limit_h = 100000;
#ifdef _WIN32
            RECT work{};
            if (SystemParametersInfoA(SPI_GETWORKAREA, 0, &work, 0)) {
                limit_w = (int)((work.right - work.left - 20) / g_screen);
                limit_h = (int)((work.bottom - work.top - 20) / g_screen);
            }
#elif defined(__linux__)
            int cw = 0, ch = 0;
            if (void *win = w.window().value()) linux_desk(win, 8, &limit_w, &limit_h, &cw, &ch);
#endif
            if (pw > limit_w) pw = limit_w;
            if (ph > limit_h) ph = limit_h;
            w.dispatch([&w, pw, ph] { w.set_size(pw, ph, WEBVIEW_HINT_NONE); });
            char out[96];
            snprintf(out, sizeof out, "{\"w\":%d,\"h\":%d}", pw, ph);
            return out;
        });

        w.bind("hostPrefs", [](const std::string &) -> std::string {
            FILE *f = fopen(pref_path().c_str(), "rb");
            if (!f) return "{}";
            char line[128] = "";
            const char *got = fgets(line, sizeof line, f);
            fclose(f);
            if (!got) return "{}";
            const char *eq = strchr(line, '=');
            if (!eq || strncmp(line, "scale", 5) != 0) return "{}";
            std::string v(eq + 1);
            while (!v.empty() && (v.back() == '\n' || v.back() == '\r' || v.back() == ' ')) v.pop_back();
            return "{\"scale\":" + json_quote(v) + "}";
        });

        w.bind("hostSavePrefs", [](const std::string &req) -> std::string {
            const std::string v = gs_json_arg(req, 0);
            if (v.size() > 8) return "{\"ok\":false}";
            FILE *f = fopen(pref_path().c_str(), "wb");
            if (!f) return "{\"ok\":false}";
            fprintf(f, "scale=%s\n", v.c_str());
            fclose(f);
            return "{\"ok\":true}";
        });

        w.bind("hostAssets", [](const std::string &) -> std::string {
            if (folder_has_art("world"))    return "{\"lang\":\"world\"}";
            if (folder_has_art("japanese")) return "{\"lang\":\"japanese\"}";
            return "{\"lang\":null}";
        });

        /* One picture from the assets folder, as base64. */
        w.bind("hostImage", [](const std::string &req) -> std::string {
            const std::string lang = gs_json_arg(req, 0), file = gs_json_arg(req, 1);
            if ((lang != "world" && lang != "japanese") || file.find("..") != std::string::npos)
                return "{\"ok\":false}";
            for (char ch : file)
                if (!(isalnum((unsigned char)ch) || ch == '/' || ch == '-' || ch == '_' || ch == '.'))
                    return "{\"ok\":false}";
            const std::string path = g_assets + "/" + lang + "/" + file;
            FILE *f = fopen(path.c_str(), "rb");
            if (!f) return "{\"ok\":false}";
            fseek(f, 0, SEEK_END); const long len = ftell(f); fseek(f, 0, SEEK_SET);
            std::vector<uint8_t> buf((size_t)(len > 0 ? len : 0));
            const size_t got = buf.empty() ? 0 : fread(buf.data(), 1, buf.size(), f);
            fclose(f);
            if (got != buf.size() || buf.empty()) return "{\"ok\":false}";
            return "{\"ok\":true,\"data\":\"" + b64_encode(buf.data(), buf.size()) + "\"}";
        });

        /* The card to open at startup; DOGEHIRO_CARD and DOGEHIRO_TAB are for screenshots. */
        w.bind("hostStartCard", [](const std::string &) -> std::string {
            const char *env = getenv("DOGEHIRO_CARD");
            if (env && *env) g_card_path = env;
            if (g_card_path.empty()) return "{\"ok\":false}";
            const std::string p = g_card_path;
            FILE *f = fopen(p.c_str(), "rb");
            if (!f) return "{\"ok\":false}";
            std::vector<uint8_t> buf(4096);
            const size_t n = fread(buf.data(), 1, buf.size(), f);
            fclose(f);
            if (n != 2048) return "{\"ok\":false}";
            const char *tab = getenv("DOGEHIRO_TAB");
            return "{\"ok\":true,\"tab\":" + json_quote(tab ? tab : "") +
                   ",\"name\":" + json_quote(base_name(p)) +
                   ",\"data\":\"" + b64_encode(buf.data(), n) + "\"}";
        });

        w.bind("hostOpenCard", [](const std::string &) -> std::string {
            std::string path;
            if (!gs_dialog_available())
                return fail("This system has no file window. Start the editor with the card: "
                            "dogehiro /path/to/card.bin");
            if (!gs_dialog_open("Open card", "Card image (*.bin)\0*.bin\0All files\0*.*\0", path))
                return "{\"ok\":false}";
            FILE *f = fopen(path.c_str(), "rb");
            if (!f) return fail("Could not open " + base_name(path));
            std::vector<uint8_t> buf(4096);
            const size_t n = fread(buf.data(), 1, buf.size(), f);
            fclose(f);
            if (n != 2048) return fail(base_name(path) + " is " + std::to_string(n) + " bytes; a card is 2048.");
            g_card_path = path;
            return "{\"ok\":true,\"name\":" + json_quote(base_name(path)) +
                   ",\"data\":\"" + b64_encode(buf.data(), n) + "\"}";
        });

        w.bind("hostSaveCard", [](const std::string &req) -> std::string {
            const std::string suggested = gs_json_arg(req, 0);
            std::vector<uint8_t> bytes;
            if (!b64_decode(gs_json_arg(req, 1), bytes) || bytes.size() != 2048)
                return fail("The card did not come back whole; nothing was written.");
            std::string path;
            const std::string start = g_card_path.empty() ? suggested : g_card_path;
            if (!gs_dialog_available()) {
                /* no file window: write back to the card named on the command line */
                if (g_card_path.empty())
                    return fail("This system has no file window. Start the editor with the card: "
                                "dogehiro /path/to/card.bin");
                path = g_card_path;
            } else if (!gs_dialog_save("Save card", start.c_str(), path)) return "{\"ok\":false}";
            FILE *f = fopen(path.c_str(), "wb");
            if (!f) return fail("Could not write " + base_name(path));
            const size_t n = fwrite(bytes.data(), 1, bytes.size(), f);
            fclose(f);
            if (n != bytes.size()) return fail("Only part of the card could be written.");
            g_card_path = path;
            return "{\"ok\":true,\"name\":" + json_quote(base_name(path)) + "}";
        });

        w.bind("hostPickGame", [](const std::string &) -> std::string {
            std::string path;
            if (!gs_dialog_open("Pick your copy of Ghost Squad",
                                "Game image\0*.bin;*.iso;*.xts\0All files\0*.*\0", path))
                return "{\"ok\":false}";
            GsArchive found;
            std::string err;
            if (!gs_find_archive(path.c_str(), &found, &err, NULL, NULL)) return fail(err);
            g_game = found;
            g_hud = GsArchive();
            gs_find_hud_archive(path.c_str(), &g_hud);   /* optional: only a game image has it */
            return "{\"ok\":true,\"name\":" + json_quote(base_name(path)) +
                   ",\"hud\":" + (g_hud.ok() ? "true" : "false") + "}";
        });

        w.bind("hostImport", [](const std::string &req) -> std::string {
            const std::string lang = gs_json_arg(req, 0);
            if (!g_game.ok()) return fail("No game picked yet.");
            std::string err;
            const std::string dir = g_assets + "/" + lang;
            if (!gs_export_assets(g_game, dir.c_str(), lang == "japanese", &err)) return fail(err);
            std::string ignored;
            const bool hidden = g_hud.ok() && gs_export_hidden(g_hud, dir.c_str(), &ignored);
            return std::string("{\"ok\":true,\"hidden\":") + (hidden ? "true" : "false") + "}";
        });

        w.set_html(reinterpret_cast<const char *>(UI_INDEX_HTML));
        w.run();
    } catch (const webview::exception &e) {
        fprintf(stderr, "the system web engine could not start: %s\n", e.what());
        return 1;
    }
    return 0;
}
