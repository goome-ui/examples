#include <stdio.h>
#include <string.h>

#include "goome.h"

#define RING_W 292
#define RING 180

static uint32_t ring_px[RING_W * RING * 4];

static goome_bitmap draw_ring(goome_font *font, double scale, double left, uint32_t color, const char *text)
{
    int w = (int)(RING_W * scale + 0.5), h = (int)(RING * scale + 0.5);
    memset(ring_px, 0, sizeof(uint32_t) * (size_t)w * h);
    goome_canvas c = {.px = ring_px, .w = w, .h = h, .stride = w, .scale = scale, .kx1 = w, .ky1 = h};
    double cx = RING_W / 2.0, cy = RING / 2.0, pi = 3.14159265358979;
    goome_draw_ring(&c, cx, cy, cy - 12, 14, 0x30FFFFFF);
    if (left > 0.001) goome_draw_arc(&c, cx, cy, cy - 12, 14, -pi / 2, -pi / 2 + 2 * pi * left, color);
    goome_text(&c, font, 40, cx - goome_text_measure(font, 40, text, NULL) / 2, cy - goome_text_line_height(font, 40) / 2, text, 0xFFFFFFFF);
    return (goome_bitmap){.px = ring_px, .w = w, .h = h, .stride = w};
}

int main(void)
{
    int err;
    goome *g = goome_open(&(goome_opts){.title = "Timer", .mode = GOOME_MODE_WINDOW, .width = 320, .height = 380,
                                        .window_flags = GOOME_WINDOW_ON_TOP | GOOME_WINDOW_CENTER},
                          &err);
    if (!g) return fprintf(stderr, "no window\n"), 1;
    goome_ui *u = goome_ui_new(g);
    static const char *const kinds[] = {"Focus", "Break", "Long break"};
    static const int minutes[] = {25, 5, 15};
    int kind = 0, running = 0, open = 1, done = 0;
    long length = minutes[0] * 60000L, left = length, last = goome_now_ms();
    while (open) {
        goome_event ev;
        while (goome_poll(g, &ev, running ? 100 : 500) > 0) {
            if (ev.type == GOOME_QUIT || ev.type == GOOME_CLOSE) open = 0;
            goome_ui_feed(u, &ev);
        }
        if (!open) break;
        long now = goome_now_ms();
        if (running) left -= now - last;
        last = now;
        if (running && left <= 0) left = 0, running = 0, done++;
        goome_canvas *c = goome_ui_begin(u, 0);
        if (!c) continue;
        goome_ui_app(u, "Timer", 0);
        int was = kind;
        goome_ui_tabs(u, "kind", kinds, 3, &kind);
        if (kind != was) running = 0, length = left = minutes[kind] * 60000L;
        char text[24];
        long secs = (left + 999) / 1000;
        snprintf(text, sizeof(text), "%02ld:%02ld", secs / 60, secs % 60);
        uint32_t color = kind == 0 ? goome_ui_theme(u)->accent : 0xFF5FB8F0;
        goome_bitmap ring = draw_ring(goome_ui_theme(u)->font, c->scale, (double)left / length, color, text);
        goome_ui_space(u, 6);
        goome_ui_image(u, &ring, RING_W, RING, 0);
        goome_ui_space(u, 6);
        goome_ui_row(u, 2);
        if (goome_ui_button(u, running ? "Pause" : "Start")) running = !running;
        if (goome_ui_button(u, "Reset")) running = 0, left = length;
        goome_ui_label(u, "%d done today", done);
        goome_ui_app_end(u);
        goome_ui_end(u);
    }
    goome_ui_free(u);
    goome_close(g);
}
