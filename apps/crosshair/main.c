#define _DEFAULT_SOURCE
#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "goome.h"

#define MAX_MONS 8

enum { HOTKEY_SHOW = 1, HOTKEY_SETTINGS };
enum { CROSS, CROSS_DOT, DOT, CIRCLE, CIRCLE_DOT, TEE, CHEVRON, SQUARE, STYLES };

static const char *const style_names[STYLES] = {"Cross", "Cross + dot", "Dot", "Circle", "Circle + dot", "T", "Chevron", "Square"};
static const uint32_t swatches[] = {0xFF6FDC5F, 0xFF00FFFF, 0xFFFF3B5C, 0xFFFFE14D, 0xFFFF7BE5, 0xFFFFFFFF, 0xFF4F8BFF, 0xFFFF9A3C};

typedef struct {
    int style, size, thick, gap, outline, mon, ox, oy, shown;
    double opacity;
    uint32_t color;
} xhair;

static const xhair defaults = {CROSS, 8, 2, 4, 1, 0, 0, 0, 1, 1.0, 0xFF6FDC5F};

static uint32_t with_alpha(uint32_t c, double a)
{
    if (a < 0) a = 0;
    if (a > 1) a = 1;
    return (uint32_t)(((c >> 24) & 0xFF) * a + 0.5) << 24 | (c & 0xFFFFFF);
}

static void bar(goome_canvas *c, double x, double y, double w, double h, double grow, uint32_t col)
{
    goome_draw_rect(c, x - grow, y - grow, w + grow * 2, h + grow * 2, col);
}

static void shape(goome_canvas *c, const xhair *x, double cx, double cy, double grow, uint32_t col)
{
    double s = x->size, t = x->thick, gap = x->gap, half = t / 2;
    int arms = x->style == CROSS || x->style == CROSS_DOT || x->style == TEE;
    int dot = x->style == DOT || x->style == CROSS_DOT || x->style == CIRCLE_DOT;
    if (arms) {
        bar(c, cx - gap - s, cy - half, s, t, grow, col);
        bar(c, cx + gap, cy - half, s, t, grow, col);
        bar(c, cx - half, cy + gap, t, s, grow, col);
        if (x->style != TEE) bar(c, cx - half, cy - gap - s, t, s, grow, col);
    }
    if (x->style == CIRCLE || x->style == CIRCLE_DOT) goome_draw_ring(c, cx, cy, gap + s, t + grow * 2, col);
    if (x->style == CHEVRON) {
        double a = gap, w = t + grow * 2;
        goome_draw_line(c, cx - s, cy + a + s, cx, cy + a, w, col);
        goome_draw_line(c, cx, cy + a, cx + s, cy + a + s, w, col);
    }
    if (x->style == SQUARE) {
        double r = gap + s;
        goome_draw_frame(c, cx - r - grow, cy - r - grow, r * 2 + grow * 2, r * 2 + grow * 2, t + grow * 2, col);
    }
    if (dot) {
        double r = x->style == DOT ? (t + s / 4) : t * 0.75 + 0.75;
        goome_draw_circle(c, cx, cy, r + grow, col);
    }
}

static void draw_xhair(goome_canvas *c, const xhair *x, double cx, double cy)
{
    cx = (int)cx + (x->thick % 2 ? 0.5 : 0), cy = (int)cy + (x->thick % 2 ? 0.5 : 0);
    if (x->outline) shape(c, x, cx, cy, 1, with_alpha(0xD0000000, x->opacity));
    shape(c, x, cx, cy, 0, with_alpha(x->color, x->opacity));
}

static void config_dir(char *out, int cap)
{
#ifdef _WIN32
    const char *base = getenv("APPDATA");
    snprintf(out, cap, "%s\\goome-crosshair", base ? base : ".");
    _mkdir(out);
#elif defined(__APPLE__)
    const char *home = getenv("HOME");
    snprintf(out, cap, "%s/Library/Application Support/goome-crosshair", home ? home : ".");
    mkdir(out, 0700);
#else
    const char *xdg = getenv("XDG_CONFIG_HOME"), *home = getenv("HOME");
    if (xdg && *xdg) snprintf(out, cap, "%s/goome-crosshair", xdg);
    else snprintf(out, cap, "%s/.config/goome-crosshair", home ? home : ".");
    mkdir(out, 0700);
#endif
}

static int load(const char *path, xhair *x)
{
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        unsigned int col;
        double d;
        sscanf(line, "style %d", &x->style);
        sscanf(line, "size %d", &x->size);
        sscanf(line, "thick %d", &x->thick);
        sscanf(line, "gap %d", &x->gap);
        sscanf(line, "outline %d", &x->outline);
        sscanf(line, "monitor %d", &x->mon);
        sscanf(line, "offset %d %d", &x->ox, &x->oy);
        sscanf(line, "shown %d", &x->shown);
        if (sscanf(line, "opacity %lf", &d) == 1) x->opacity = d;
        if (sscanf(line, "color %x", &col) == 1) x->color = col;
    }
    fclose(f);
    if (x->style < 0 || x->style >= STYLES) x->style = CROSS;
    return 1;
}

static void save(const char *path, const xhair *x)
{
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "style %d\nsize %d\nthick %d\ngap %d\noutline %d\nmonitor %d\noffset %d %d\nshown %d\nopacity %.2f\ncolor %08X\n", x->style, x->size, x->thick, x->gap,
            x->outline, x->mon, x->ox, x->oy, x->shown, x->opacity, x->color);
    fclose(f);
}

static int swatch_row(goome_ui *u, uint32_t *color)
{
    int changed = 0, n = (int)(sizeof(swatches) / sizeof(*swatches));
    for (int i = 0; i < n; i++) {
        char id[16];
        snprintf(id, sizeof(id), "sw%d", i);
        if (i) goome_ui_same_line(u);
        goome_ui_item it;
        goome_ui_custom(u, id, 30, 30, 0, &it);
        if (it.clicked) *color = swatches[i], changed = 1;
        goome_canvas *c = goome_ui_canvas(u);
        int on = (*color & 0xFFFFFF) == (swatches[i] & 0xFFFFFF);
        double cx = it.x + 15, cy = it.y + 15, r = it.hovered || on ? 10 : 9;
        if (on) goome_draw_ring(c, cx, cy, 13.5, 2, 0xFFFFFFFF);
        goome_draw_circle(c, cx, cy, r, swatches[i]);
        goome_draw_ring(c, cx, cy, r, 1, 0x40000000);
    }
    return changed;
}

static void preview(goome_ui *u, const xhair *x)
{
    goome_ui_item it;
    goome_ui_custom(u, "preview", 0, 130, GOOME_ITEM_NO_HIT, &it);
    goome_canvas *c = goome_ui_canvas(u);
    goome_radii r = goome_radius(10);
    goome_draw_rrect_grad(c, it.x, it.y, it.w, it.h, r, 0xFF5B7A99, 0xFF2E3B2C, 0);
    goome_draw_rrect(c, it.x, it.y + it.h * 0.62, it.w, it.h * 0.38, (goome_radii){0, 0, 10, 10}, 0xFF3E4A32);
    goome_draw_rect(c, it.x + it.w * 0.62, it.y + it.h * 0.30, it.w * 0.16, it.h * 0.32, 0xFF7A6A58);
    goome_draw_rect(c, it.x + it.w * 0.66, it.y + it.h * 0.38, it.w * 0.05, it.h * 0.08, 0xFF2A2420);
    goome_draw_rect(c, it.x + it.w * 0.18, it.y + it.h * 0.42, it.w * 0.10, it.h * 0.20, 0xFF8C8F96);
    goome_draw_rrect_line(c, it.x + 0.5, it.y + 0.5, it.w - 1, it.h - 1, r, 1, 0x30FFFFFF);
    goome_clip_push(c, it.x, it.y, it.w, it.h);
    xhair big = *x;
    big.shown = 1;
    draw_xhair(c, &big, it.x + it.w / 2, it.y + it.h / 2);
    goome_clip_pop(c);
}

int main(int argc, char **argv)
{
    char dir[1024], cfg[1100];
    config_dir(dir, sizeof(dir));
    if (argc >= 3 && !strcmp(argv[1], "--send")) {
        int rc = goome_send(dir, argv[2]);
        if (rc != GOOME_OK) fprintf(stderr, "crosshair: %s, is it running?\n", goome_strerror(rc));
        return rc != GOOME_OK;
    }
    int err;
    goome *g = goome_open(&(goome_opts){.title = "crosshair", .no_capture = 1, .state_dir = dir}, &err);
    if (!g) {
        fprintf(stderr, "crosshair: cant open an overlay here, %s\n", goome_strerror(err));
        return 1;
    }
    goome_ui *u = goome_ui_new(g);
    int test = !strcmp(goome_backend_name(g), "test");
    snprintf(cfg, sizeof(cfg), test ? "crosshair.txt" : "%s/crosshair.txt", dir);
    xhair x = defaults;
    int first_run = !load(cfg, &x);
    goome_hotkey(g, "alt+shift+x", HOTKEY_SHOW);
    goome_hotkey(g, "alt+shift+c", HOTKEY_SETTINGS);
    goome_ui_theme_make(goome_ui_theme(u), 0xFF6FDC5F, 1, 10);

    goome_monitor mons[MAX_MONS];
    int nmons = goome_monitors(g, mons, MAX_MONS);
    if (x.mon >= nmons) x.mon = 0;
    int settings = first_run || !x.shown, drawn_mon = x.mon, running = 1, dirty = 1, pending_save = 0;
    double wx = 0, wy = 0, ww = 340, wh = 0;
    if (nmons > 0) wx = mons[x.mon].area.x + 60, wy = mons[x.mon].area.y + 60;
    goome_pacer pace = {0};
    while (running) {
        goome_event ev;
        int wait = settings || goome_ui_animating(u) || test ? goome_pace(&pace, 60) : (pending_save ? 500 : -1);
        while (goome_poll(g, &ev, wait) > 0) {
            wait = 0;
            dirty = 1;
            if (ev.type == GOOME_QUIT) running = 0;
            if (ev.type == GOOME_MONITORS) {
                nmons = goome_monitors(g, mons, MAX_MONS);
                if (x.mon >= nmons) x.mon = 0;
            }
            if ((ev.type == GOOME_HOTKEY && ev.id == HOTKEY_SHOW) || (ev.type == GOOME_CONTROL && !strcmp(ev.msg, "toggle"))) x.shown = !x.shown, pending_save = 1;
            if ((ev.type == GOOME_HOTKEY && ev.id == HOTKEY_SETTINGS) || (ev.type == GOOME_CONTROL && !strcmp(ev.msg, "settings"))) settings = !settings;
            if (ev.type == GOOME_CONTROL && !strcmp(ev.msg, "quit")) running = 0;
            goome_ui_feed(u, &ev);
        }
        if (!running) break;
        if (pending_save && !settings) save(cfg, &x), pending_save = 0;
        if (!dirty && !test && !settings && !goome_ui_animating(u)) continue;
        dirty = 0;
        if (nmons < 1) continue;
        if (drawn_mon != x.mon && drawn_mon < nmons) {
            goome_canvas *old = goome_begin(g, drawn_mon);
            if (old) goome_clear(old), goome_end(g, old, NULL, 0);
        }
        drawn_mon = x.mon;
        goome_canvas *c = goome_ui_begin(u, x.mon);
        if (!c) continue;
        goome_rect a = mons[x.mon].area;
        if (x.shown || settings) draw_xhair(c, &x, a.x + a.w / 2.0 + x.ox, a.y + a.h / 2.0 + x.oy);
        if (settings) {
            int open = 1, ch = 0;
            if (goome_ui_window(u, "Crosshair", &wx, &wy, &ww, &wh, GOOME_WIN_MOVE | GOOME_WIN_CLOSE, &open)) {
                preview(u, &x);
                ch |= goome_ui_combo(u, "Style", &x.style, style_names, STYLES);
                ch |= swatch_row(u, &x.color);
                ch |= goome_ui_color(u, "Color", &x.color);
                goome_ui_row(u, 2);
                ch |= goome_ui_slider_int(u, "Size", &x.size, 1, 60);
                ch |= goome_ui_slider_int(u, "Thickness", &x.thick, 1, 12);
                goome_ui_row(u, 2);
                ch |= goome_ui_slider_int(u, "Gap", &x.gap, 0, 30);
                ch |= goome_ui_slider(u, "Opacity", &x.opacity, 0.1, 1, "%.2f");
                goome_ui_row(u, 2);
                ch |= goome_ui_toggle(u, "Outline", &x.outline);
                ch |= goome_ui_toggle(u, "Shown", &x.shown);
                goome_ui_row(u, 2);
                ch |= goome_ui_drag_int(u, "Move X", &x.ox, 0.5, -400, 400);
                ch |= goome_ui_drag_int(u, "Move Y", &x.oy, 0.5, -400, 400);
                if (nmons > 1) {
                    const char *names[MAX_MONS];
                    char labels[MAX_MONS][80];
                    for (int i = 0; i < nmons && i < MAX_MONS; i++) {
                        snprintf(labels[i], sizeof(labels[i]), "%d: %.40s (%dx%d)", i + 1, mons[i].name, mons[i].area.w, mons[i].area.h);
                        names[i] = labels[i];
                    }
                    if (goome_ui_combo(u, "Monitor", &x.mon, names, nmons < MAX_MONS ? nmons : MAX_MONS)) {
                        ch = 1;
                        wx = mons[x.mon].area.x + 60, wy = mons[x.mon].area.y + 60;
                    }
                }
                goome_ui_separator(u);
                goome_ui_wrapped(u, "alt+shift+x shows or hides it, alt+shift+c opens this window. clicks go straight through the crosshair to your game.");
                goome_ui_space(u, 4);
                goome_ui_row(u, 3);
                if (goome_ui_button(u, "Reset")) {
                    int mon = x.mon;
                    x = defaults, x.mon = mon, ch = 1;
                }
                if (goome_ui_button(u, "Done")) open = 0;
                if (goome_ui_button(u, "Quit")) running = 0;
            }
            goome_ui_window_end(u);
            if (ch) pending_save = 1;
            if (!open) settings = 0, dirty = 1;
        }
        goome_ui_end(u);
    }
    save(cfg, &x);
    goome_ui_free(u);
    goome_close(g);
    return 0;
}
