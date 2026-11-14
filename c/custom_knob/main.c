#include <stdio.h>

#include "goome.h"

static double clampd(double v, double a, double b) { return v < a ? a : v > b ? b : v; }

static double sine(double x)
{
    while (x > 3.14159265358979) x -= 6.28318530717959;
    while (x < -3.14159265358979) x += 6.28318530717959;
    double x2 = x * x;
    return x * (1 - x2 / 6 * (1 - x2 / 20 * (1 - x2 / 42 * (1 - x2 / 72 * (1 - x2 / 110)))));
}

static double cosine(double x) { return sine(x + 1.5707963267949); }

static int knob(goome_ui *u, const char *name, double *v)
{
    goome_ui_item it;
    goome_ui_custom(u, name, 110, 130, GOOME_ITEM_DRAG_Y | GOOME_ITEM_FOCUSABLE, &it);
    double was = *v;
    int key = it.key & 0xFF;
    if (it.held) *v = clampd(*v - it.dy / 200, 0, 1);
    if (key == GOOME_K_UP) *v = clampd(*v + 0.05, 0, 1);
    if (key == GOOME_K_DOWN) *v = clampd(*v - 0.05, 0, 1);
    if (it.double_clicked) *v = 0.5;
    goome_theme *t = goome_ui_theme(u);
    goome_canvas *c = goome_ui_canvas(u);
    if (!c) return *v != was;
    double cx = it.x + it.w / 2, cy = it.y + 50, a0 = 2.356, a1 = 7.069, at = a0 + (a1 - a0) * *v;
    if (it.hovered || it.held) goome_draw_circle(c, cx, cy, 44, (t->accent & 0xFFFFFF) | 0x20000000);
    goome_draw_arc(c, cx, cy, 36, 6, a0, a1, (t->text & 0xFFFFFF) | 0x20000000);
    if (*v > 0.004) goome_draw_arc(c, cx, cy, 36, 6, a0, at, t->accent);
    goome_draw_circle(c, cx, cy, 25, t->input_bg);
    goome_draw_line(c, cx + cosine(at) * 10, cy + sine(at) * 10, cx + cosine(at) * 21, cy + sine(at) * 21, 3, t->text);
    char label[32];
    snprintf(label, sizeof(label), "%s %.0f%%", name, *v * 100);
    double tw = goome_text_measure(t->font, 14, label, NULL);
    goome_text(c, t->font, 14, cx - tw / 2, it.y + 100, label, t->text_dim);
    return *v != was;
}

int main(void)
{
    int err;
    goome *g = goome_open_app("Custom knob", 420, 260, &err);
    if (!g) return fprintf(stderr, "no window: %s\n", goome_strerror(err)), 1;
    goome_ui *u = goome_ui_new(g);
    int running = 1;
    while (running) {
        goome_event ev;
        while (goome_poll(g, &ev, 16) > 0) {
            if (ev.type == GOOME_QUIT || ev.type == GOOME_CLOSE) running = 0;
            goome_ui_feed(u, &ev);
        }
        if (!running) break;
        if (!goome_ui_begin(u, 0)) continue;
        goome_ui_app(u, "Custom knob", 0);
        double *v = goome_ui_store(u, "knobs", 3);
        if (!v[2]) v[0] = 0.3, v[1] = 0.75, v[2] = 1;
        goome_ui_label(u, "drag up or down, arrows work too, double click resets");
        knob(u, "Bass", &v[0]);
        goome_ui_same_line(u);
        knob(u, "Treble", &v[1]);
        goome_ui_app_end(u);
        goome_ui_end(u);
    }
    goome_ui_free(u);
    goome_close(g);
    return 0;
}
