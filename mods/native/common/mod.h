#ifndef GOOME_MOD_H
#define GOOME_MOD_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "MinHook.h"
#include "goome.h"
#include "goome_menu.h"
#include "goome_win32.h"

typedef struct {
    goome *g;
    goome_ui *u;
    goome_mod_menu menu;
    HWND win;
    WNDPROC orig;
    int w, h;
} goome_mod;

static goome_mod gmod;

static LRESULT CALLBACK goome_mod_proc(HWND h, UINT msg, WPARAM w, LPARAM l)
{
    if (msg == WM_KEYDOWN && w == VK_INSERT && !(l & 0x40000000)) {
        gmod.menu.show = !gmod.menu.show;
        if (gmod.menu.show) gmod.menu.open = 1;
        return 0;
    }
    int eaten = 0;
    LRESULT r = goome_win32_wndproc(gmod.g, h, msg, w, l, &eaten);
    if (eaten) return r;
    return CallWindowProcW(gmod.orig, h, msg, w, l);
}

static void goome_mod_subclass(HWND win)
{
    if (gmod.win == win) return;
    if (gmod.win && gmod.orig) SetWindowLongPtrW(gmod.win, GWLP_WNDPROC, (LONG_PTR)gmod.orig);
    gmod.win = win;
    goome_win32_init(gmod.g, win);
    goome_win32_set_ui(gmod.g, gmod.u);
    gmod.orig = (WNDPROC)SetWindowLongPtrW(win, GWLP_WNDPROC, (LONG_PTR)goome_mod_proc);
}

static int goome_mod_ensure(int w, int h)
{
    if (w <= 0 || h <= 0) return 0;
    if (gmod.g && (gmod.w != w || gmod.h != h)) gmod.w = w, gmod.h = h;
    if (gmod.g) return 1;
    gmod.g = goome_open(&(goome_opts){.mode = GOOME_MODE_OFFSCREEN, .width = w, .height = h}, NULL);
    if (!gmod.g) return 0;
    gmod.u = goome_ui_new(gmod.g);
    goome_mod_menu_init(&gmod.menu);
    gmod.w = w, gmod.h = h;
    return gmod.u != NULL;
}

static void goome_mod_frame(const char *api)
{
    goome_win32_frame(gmod.g);
    goome_mod_menu_draw(gmod.u, &gmod.menu, api);
}

static void goome_mod_teardown(void)
{
    if (gmod.win && gmod.orig) SetWindowLongPtrW(gmod.win, GWLP_WNDPROC, (LONG_PTR)gmod.orig);
    if (gmod.g) goome_win32_shutdown(gmod.g);
    if (gmod.u) goome_ui_free(gmod.u);
    if (gmod.g) goome_close(gmod.g);
    gmod.g = NULL, gmod.u = NULL, gmod.win = NULL, gmod.orig = NULL;
}

#endif
