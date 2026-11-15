#include <stdio.h>

#include "goome.h"

int main(void)
{
    int err;
    goome *g = goome_open(&(goome_opts){.title = "cat", .mode = GOOME_MODE_WINDOW, .width = 220, .height = 220,
                                        .window_flags = GOOME_WINDOW_BORDERLESS | GOOME_WINDOW_TRANSPARENT | GOOME_WINDOW_ON_TOP | GOOME_WINDOW_CENTER},
                          &err);
    if (!g) {
        fprintf(stderr, "cant make a see-through window here (%s)\n", goome_strerror(err));
        return 1;
    }
    int running = 1, pets = 0;
    long blink = goome_now_ms() + 2500;
    while (running) {
        goome_event ev;
        while (goome_poll(g, &ev, 50) > 0) {
            if (ev.type == GOOME_QUIT || ev.type == GOOME_CLOSE) running = 0;
            if (ev.type == GOOME_KEY && ev.key == GOOME_K_ESC) running = 0;
            if (ev.type == GOOME_DOWN && ev.button == 1) {
                double dx = ev.x - 110, dy = ev.y - 140;
                if (dx * dx + dy * dy < 30 * 30) pets++;
                else goome_window_drag(g, 0);
            }
            if (ev.type == GOOME_DOWN && ev.button == 3) running = 0;
        }
        if (!running) break;
        goome_canvas *c = goome_begin(g, 0);
        if (!c) continue;
        goome_clear(c);
        long now = goome_now_ms();
        int closed = now > blink && now < blink + 150;
        if (now > blink + 150) blink = now + 2000 + (now % 1700);
        goome_draw_shadow(c, 34, 50, 152, 152, 76, 14, 0x60000000);
        goome_draw_triangle(c, 40, 90, 52, 18, 100, 52, 0xFF6FDC5F);
        goome_draw_triangle(c, 180, 90, 168, 18, 120, 52, 0xFF6FDC5F);
        goome_draw_triangle(c, 54, 76, 60, 36, 88, 56, 0xFFF29BB8);
        goome_draw_triangle(c, 166, 76, 160, 36, 132, 56, 0xFFF29BB8);
        goome_draw_circle(c, 110, 126, 76, 0xFF6FDC5F);
        goome_draw_circle(c, 82, 112, closed ? 2 : 12, 0xFFFFFFFF);
        goome_draw_circle(c, 138, 112, closed ? 2 : 12, 0xFFFFFFFF);
        if (!closed) goome_draw_circle(c, 84, 115, 6, 0xFF1B1D22), goome_draw_circle(c, 140, 115, 6, 0xFF1B1D22);
        goome_draw_triangle(c, 104, 136, 116, 136, 110, 143, 0xFFF29BB8);
        goome_draw_arc(c, 102, 144, 8, 2.5, 0.3, 2.8, 0xFF1B1D22);
        goome_draw_arc(c, 118, 144, 8, 2.5, 0.3, 2.8, 0xFF1B1D22);
        if (pets) {
            char line[32];
            snprintf(line, sizeof(line), "pets: %d", pets);
            goome_draw_text(c, 110 - goome_text_width(c, line, 0) / 2, 170, line, 0, 0xFF1B1D22);
        }
        goome_end(g, c, NULL, 0);
    }
    goome_close(g);
    return 0;
}
