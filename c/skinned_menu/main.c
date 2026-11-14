#include <stdio.h>
#include <string.h>

#include "goome.h"

enum { BTN = 24, PANEL = 40, ICON = 32 };

static uint32_t btn_px[BTN * BTN], pressed_px[BTN * BTN], panel_px[PANEL * PANEL], bar_px[PANEL * PANEL];
static uint32_t star_px[ICON * ICON], close_px[ICON * ICON], gem_px[ICON * ICON];

static goome_canvas canvas_on(uint32_t *px, int w, int h)
{
    memset(px, 0, sizeof(uint32_t) * (size_t)w * h);
    return (goome_canvas){.px = px, .w = w, .h = h, .stride = w, .scale = 1, .kx1 = w, .ky1 = h};
}

static goome_bitmap bitmap_of(uint32_t *px, int w, int h) { return (goome_bitmap){.px = px, .w = w, .h = h, .stride = w}; }

static void paint(void)
{
    goome_canvas c = canvas_on(btn_px, BTN, BTN);
    goome_draw_rrect_grad(&c, 0, 0, BTN, BTN, goome_radius(6), 0xFF8A5A2B, 0xFF5C3A1A, 0);
    goome_draw_rrect_line(&c, 0.5, 0.5, BTN - 1, BTN - 1, goome_radius(6), 1, 0xFFE8B65A);
    goome_draw_rect(&c, 6, 2, BTN - 12, 1, 0x60FFFFFF);

    c = canvas_on(pressed_px, BTN, BTN);
    goome_draw_rrect_grad(&c, 0, 0, BTN, BTN, goome_radius(6), 0xFF4A2E14, 0xFF6E4622, 0);
    goome_draw_rrect_line(&c, 0.5, 0.5, BTN - 1, BTN - 1, goome_radius(6), 1, 0xFFFFD27A);

    c = canvas_on(panel_px, PANEL, PANEL);
    goome_draw_rrect(&c, 0, 0, PANEL, PANEL, goome_radius(10), 0xF0201812);
    goome_draw_rrect_line(&c, 1, 1, PANEL - 2, PANEL - 2, goome_radius(9), 2, 0xFFC8963E);
    goome_draw_rrect_line(&c, 4.5, 4.5, PANEL - 9, PANEL - 9, goome_radius(6), 1, 0x50C8963E);

    c = canvas_on(bar_px, PANEL, PANEL);
    goome_radii top = {10, 10, 0, 0};
    goome_draw_rrect_grad(&c, 0, 0, PANEL, PANEL, top, 0xFF6B4320, 0xFF3A2410, 0);
    goome_draw_rect(&c, 0, PANEL - 2, PANEL, 2, 0xFFC8963E);

    c = canvas_on(star_px, ICON, ICON);
    double s[20];
    for (int i = 0; i < 10; i++) {
        static const double dx[10] = {0, .22, .95, .36, .59, 0, -.59, -.36, -.95, -.22};
        static const double dy[10] = {-1, -.31, -.31, .12, .81, .38, .81, .12, -.31, -.31};
        s[i * 2] = 16 + dx[i] * 14, s[i * 2 + 1] = 17 + dy[i] * 14;
    }
    goome_draw_poly(&c, s, 10, 0xFFFFD27A);

    c = canvas_on(close_px, ICON, ICON);
    goome_draw_line(&c, 9, 9, 23, 23, 4, 0xFFFFD27A);
    goome_draw_line(&c, 23, 9, 9, 23, 4, 0xFFFFD27A);

    c = canvas_on(gem_px, ICON, ICON);
    double gem[8] = {16, 3, 29, 16, 16, 29, 3, 16};
    goome_draw_poly(&c, gem, 4, 0xFF5FE0D0);
    goome_draw_triangle(&c, 16, 3, 29, 16, 16, 16, 0x50FFFFFF);
}

int main(void)
{
    int err;
    goome *g = goome_open_app("Skinned menu", 560, 420, &err);
    if (!g) return fprintf(stderr, "no window: %s\n", goome_strerror(err)), 1;
    goome_ui *u = goome_ui_new(g);
    paint();
    goome_bitmap btn = bitmap_of(btn_px, BTN, BTN), panel = bitmap_of(panel_px, PANEL, PANEL), bar = bitmap_of(bar_px, PANEL, PANEL);
    goome_bitmap pressed = bitmap_of(pressed_px, BTN, BTN), star = bitmap_of(star_px, ICON, ICON), cross = bitmap_of(close_px, ICON, ICON);
    goome_bitmap gem = bitmap_of(gem_px, ICON, ICON);
    goome_theme *t = goome_ui_theme(u);
    goome_ui_theme_make(t, 0xFFE8B65A, 1, 6);
    t->bg = 0xFF15100C, t->text = 0xFFF3E3C3, t->text_dim = 0xFFB89F78;
    t->skin_button = (goome_skin){&btn, 8, 8, 8, 8};
    t->skin_input = (goome_skin){&pressed, 8, 8, 8, 8};
    t->skin_panel = (goome_skin){&panel, 12, 12, 12, 12};
    t->skin_titlebar = (goome_skin){&bar, 12, 12, 12, 4};
    t->icon[GOOME_ICON_CHECK] = &star;
    t->icon[GOOME_ICON_CLOSE] = &cross;
    t->icon[GOOME_ICON_SLIDER_KNOB] = &gem;
    t->icon_tint = 0;
    int running = 1, open = 1, torch = 1, map = 0, god = 0;
    double x = 40, y = 30, w = 330, h = 0, gold = 250, light = 0.7;
    while (running) {
        goome_event ev;
        while (goome_poll(g, &ev, 16) > 0) {
            if (ev.type == GOOME_QUIT || ev.type == GOOME_CLOSE) running = 0;
            goome_ui_feed(u, &ev);
        }
        if (!running) break;
        if (!goome_ui_begin(u, 0)) continue;
        if (goome_ui_window(u, "Camp menu", &x, &y, &w, &h, GOOME_WIN_MOVE | GOOME_WIN_CLOSE, &open)) {
            goome_ui_checkbox(u, "Torch lit", &torch);
            goome_ui_checkbox(u, "Show map", &map);
            goome_ui_checkbox(u, "God mode", &god);
            goome_ui_slider(u, "Gold", &gold, 0, 999, "%.0f");
            goome_ui_slider(u, "Firelight", &light, 0, 1, "%.2f");
            goome_ui_row(u, 2);
            if (goome_ui_button(u, "Rest")) gold -= 10;
            if (goome_ui_button(u, "Leave camp")) open = 0;
        }
        goome_ui_window_end(u);
        if (!open && goome_ui_button(u, "Open the camp menu")) open = 1;
        goome_ui_end(u);
    }
    goome_ui_free(u);
    goome_close(g);
    return 0;
}
