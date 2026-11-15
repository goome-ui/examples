#include <stdio.h>

#include "goome.h"

#define HISTORY 90

int main(void)
{
    int err;
    goome *g = goome_open(&(goome_opts){.title = "fps hud", .no_capture = 1}, &err);
    if (!g) {
        fprintf(stderr, "fps hud: %s\n", goome_strerror(err));
        return 1;
    }
    goome_font *font = goome_font_system(NULL);
    goome_hotkey(g, "alt+f", 1);
    float ms[HISTORY] = {0};
    int at = 0, running = 1, big = 0, fps_cap = 60;
    long last = goome_now_ms();
    goome_pacer pace = {0};
    while (running) {
        goome_event ev;
        while (goome_poll(g, &ev, goome_pace(&pace, fps_cap)) > 0) {
            if (ev.type == GOOME_QUIT) running = 0;
            if (ev.type == GOOME_HOTKEY) big = !big;
            if (ev.type == GOOME_DOWN) fps_cap = fps_cap == 60 ? 144 : fps_cap == 144 ? 30 : 60;
        }
        if (!running) break;
        long now = goome_now_ms();
        ms[at] = (float)(now - last);
        at = (at + 1) % HISTORY;
        last = now;
        float sum = 0, worst = 0;
        for (int i = 0; i < HISTORY; i++) {
            sum += ms[i];
            if (ms[i] > worst) worst = ms[i];
        }
        double avg = sum / HISTORY, fps = avg > 0 ? 1000 / avg : 0;

        goome_monitor m;
        if (goome_monitors(g, &m, 1) < 1) continue;
        goome_canvas *c = goome_begin(g, 0);
        if (!c) continue;
        goome_clear(c);
        double x = m.area.x + 16, y = m.area.y + 16, w = big ? 300 : 130, h = big ? 120 : 46;
        goome_draw_round_rect(c, x, y, w, h, 10, 0xC0101216);
        char line[64];
        snprintf(line, sizeof(line), "%.0f fps", fps);
        uint32_t color = fps >= 55 ? 0xFF6FDC5F : fps >= 30 ? 0xFFF0C24F : 0xFFFF6B6B;
        goome_text(c, font, 20, x + 12, y + 10, line, color);
        if (big) {
            snprintf(line, sizeof(line), "avg %.1f ms  worst %.0f ms  cap %d", avg, worst, fps_cap);
            goome_text(c, font, 12, x + 12, y + 38, line, 0xFF9AA3B2);
            double gx = x + 12, gy = y + 58, gw = w - 24, gh = h - 70, top = 50;
            goome_draw_rect(c, gx, gy + gh - gh * 16.7 / top, gw, 1, 0x406FDC5F);
            double pts[HISTORY * 2];
            for (int i = 0; i < HISTORY; i++) {
                double v = ms[(at + i) % HISTORY];
                pts[i * 2] = gx + gw * i / (HISTORY - 1);
                pts[i * 2 + 1] = gy + gh - gh * (v > top ? top : v) / top;
            }
            for (int i = 1; i < HISTORY; i++) goome_draw_line(c, pts[i * 2 - 2], pts[i * 2 - 1], pts[i * 2], pts[i * 2 + 1], 1.5, color);
        }
        goome_rect hit = {(int)x, (int)y, (int)w, (int)h};
        goome_end(g, c, &hit, 1);
    }
    goome_font_free(font);
    goome_close(g);
    return 0;
}
