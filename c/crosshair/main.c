#include <stdio.h>

#include "goome.h"

enum { CROSS, DOT, CIRCLE, STYLES };

int main(void)
{
    int err;
    goome *g = goome_open(&(goome_opts){.title = "crosshair", .no_capture = 1}, &err);
    if (!g) {
        puts("no overlay here, sorry");
        return 1;
    }
    goome_hotkey(g, "alt+x", 1);
    goome_hotkey(g, "alt+up", 2);
    goome_hotkey(g, "alt+down", 3);
    goome_hotkey(g, "alt+h", 4);
    static const uint32_t colors[] = {0xFF6FDC5F, 0xFFFF4F6B, 0xFF4FC3FF, 0xFFFFFFFF};
    int style = CROSS, size = 12, color = 0, shown = 1, running = 1;
    while (running) {
        goome_event ev;
        int changed = 0;
        while (goome_poll(g, &ev, changed ? 0 : 1000) > 0) {
            if (ev.type == GOOME_QUIT) running = 0;
            if (ev.type == GOOME_HOTKEY) {
                if (ev.id == 1) style = (style + 1) % STYLES, color = (color + 1) % 4;
                if (ev.id == 2 && size < 40) size += 2;
                if (ev.id == 3 && size > 4) size -= 2;
                if (ev.id == 4) shown = !shown;
            }
            changed = 1;
        }
        if (!running) break;
        goome_monitor m;
        if (goome_monitors(g, &m, 1) < 1) continue;
        goome_canvas *c = goome_begin(g, 0);
        if (!c) continue;
        goome_clear(c);
        double cx = m.area.x + m.area.w / 2.0, cy = m.area.y + m.area.h / 2.0, s = size;
        uint32_t col = colors[color], edge = 0xA0000000;
        if (shown && style == CROSS) {
            for (int pass = 0; pass < 2; pass++) {
                double w = pass ? 2 : 4;
                uint32_t k = pass ? col : edge;
                goome_draw_line(c, cx - s - 4, cy, cx - 4, cy, w, k);
                goome_draw_line(c, cx + 4, cy, cx + s + 4, cy, w, k);
                goome_draw_line(c, cx, cy - s - 4, cx, cy - 4, w, k);
                goome_draw_line(c, cx, cy + 4, cx, cy + s + 4, w, k);
            }
        } else if (shown && style == DOT) {
            goome_draw_circle(c, cx, cy, s / 4 + 1.5, edge);
            goome_draw_circle(c, cx, cy, s / 4, col);
        } else if (shown) {
            goome_draw_ring(c, cx, cy, s, 4, edge);
            goome_draw_ring(c, cx, cy, s, 2, col);
            goome_draw_circle(c, cx, cy, 1.5, col);
        }
        goome_end(g, c, NULL, 0);
    }
    goome_close(g);
    return 0;
}
