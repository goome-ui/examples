#include <stdio.h>
#include <time.h>

#include "goome.h"

static const double pi = 3.14159265358979;

static double cosine(double a)
{
    while (a > pi) a -= 2 * pi;
    while (a < -pi) a += 2 * pi;
    double x2 = a * a, term = 1, sum = 1;
    for (int i = 1; i < 12; i++) term *= -x2 / ((2 * i - 1) * (2 * i)), sum += term;
    return sum;
}

static void hand(goome_canvas *c, double cx, double cy, double len, double turns, double width, uint32_t color)
{
    double a = turns * 2 * pi - pi / 2;
    goome_draw_line(c, cx, cy, cx + cosine(a) * len, cy + cosine(a - pi / 2) * len, width, color);
}

int main(void)
{
    int err;
    goome *g = goome_open(&(goome_opts){.title = "clock", .no_capture = 1}, &err);
    if (!g) return fprintf(stderr, "no overlay: %s\n", goome_strerror(err)), 1;
    goome_font *font = goome_font_system(NULL);
    goome_hotkey(g, "alt+c", 1);
    static const char *const days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    static const char *const months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    int running = 1, shown = 1;
    while (running) {
        goome_event ev;
        while (goome_poll(g, &ev, 250) > 0) {
            if (ev.type == GOOME_QUIT) running = 0;
            if (ev.type == GOOME_HOTKEY) shown = !shown;
        }
        if (!running) break;
        goome_monitor m;
        if (goome_monitors(g, &m, 1) < 1) continue;
        goome_canvas *c = goome_begin(g, 0);
        if (!c) continue;
        goome_clear(c);
        if (shown) {
            time_t now = time(NULL);
            struct tm *t = localtime(&now);
            double w = 250, h = 96, x = m.area.x + m.area.w - w - 24, y = m.area.y + 24, cx = x + 48, cy = y + h / 2;
            goome_draw_shadow(c, x, y, w, h, 18, 16, 0x60000000);
            goome_draw_round_rect(c, x, y, w, h, 18, 0xE01B1D22);
            goome_draw_circle(c, cx, cy, 34, 0xFF2A2E36);
            for (int i = 0; i < 12; i++) {
                double a = i * pi / 6;
                goome_draw_circle(c, cx + cosine(a) * 28, cy + cosine(a - pi / 2) * 28, i % 3 ? 1.2 : 2, 0xFF8A93A3);
            }
            hand(c, cx, cy, 17, (t->tm_hour % 12 + t->tm_min / 60.0) / 12, 3.5, 0xFFFFFFFF);
            hand(c, cx, cy, 25, (t->tm_min + t->tm_sec / 60.0) / 60, 2.5, 0xFFFFFFFF);
            hand(c, cx, cy, 27, t->tm_sec / 60.0, 1.2, 0xFF6FDC5F);
            goome_draw_circle(c, cx, cy, 3, 0xFF6FDC5F);
            char line[64];
            snprintf(line, sizeof(line), "%02d:%02d", t->tm_hour, t->tm_min);
            goome_text(c, font, 34, x + 100, y + 14, line, 0xFFFFFFFF);
            snprintf(line, sizeof(line), "%s, %d %s", days[t->tm_wday], t->tm_mday, months[t->tm_mon]);
            goome_text(c, font, 14, x + 102, y + 58, line, 0xFF9AA3B2);
        }
        goome_end(g, c, NULL, 0);
    }
    goome_font_free(font);
    goome_close(g);
    return 0;
}
