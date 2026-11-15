#ifndef GOOME_MOD_MENU_H
#define GOOME_MOD_MENU_H

#include "goome.h"

typedef struct {
    double x, y, w, h, speed, fov;
    int open, show, noclip, wire, tab;
    char player[48];
} goome_mod_menu;

static void goome_mod_menu_init(goome_mod_menu *m)
{
    m->x = 40, m->y = 40, m->w = 300, m->h = 0;
    m->speed = 1, m->fov = 90, m->open = 1, m->show = 1;
    m->player[0] = 0;
}

static void goome_mod_menu_draw(goome_ui *u, goome_mod_menu *m, const char *api)
{
    static const char *const tabs[] = {"Cheats", "View", "About"};
    if (!m->show) return;
    if (!goome_ui_begin(u, 0)) return;
    if (goome_ui_window(u, "goome mod menu", &m->x, &m->y, &m->w, &m->h, GOOME_WIN_MOVE | GOOME_WIN_CLOSE, &m->open)) {
        goome_ui_label(u, "drawn by the %s mod, Insert hides it", api);
        goome_ui_tabs(u, "t", tabs, 3, &m->tab);
        if (m->tab == 0) {
            goome_ui_toggle(u, "Noclip", &m->noclip);
            goome_ui_slider(u, "Move speed", &m->speed, 0.25, 8, "%.2fx");
            goome_ui_input(u, "Name tag", m->player, sizeof(m->player));
        } else if (m->tab == 1) {
            goome_ui_slider(u, "FOV", &m->fov, 60, 120, "%.0f");
            goome_ui_toggle(u, "Wireframe", &m->wire);
        } else {
            goome_ui_wrapped(u, "Single player and modding only. Anything with anti-cheat will ban you.");
        }
    }
    goome_ui_window_end(u);
    if (!m->open) m->show = 0;
    goome_ui_end(u);
}

#endif
