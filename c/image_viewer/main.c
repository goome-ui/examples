#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "goome.h"

typedef struct {
    goome_bitmap pic;
    int loaded;
    char name[256];
    double zoom, shown, x, y;
} viewer;

static void open_file(viewer *v, const char *path)
{
    goome_bitmap b;
    if (goome_bitmap_load(path, &b) != GOOME_OK) {
        fprintf(stderr, "cant open %s: %s\n", path, goome_bitmap_error());
        return;
    }
    if (v->loaded) goome_bitmap_free(&v->pic);
    v->pic = b, v->loaded = 1, v->zoom = 0, v->x = v->y = 0;
    const char *slash = strrchr(path, '/'), *back = strrchr(path, '\\');
    if (back > slash) slash = back;
    snprintf(v->name, sizeof(v->name), "%s", slash ? slash + 1 : path);
}

int main(int argc, char **argv)
{
    int err;
    goome *g = goome_open_app("Image viewer", 720, 500, &err);
    if (!g) {
        fprintf(stderr, "cant open a window: %s\n", goome_strerror(err));
        return 1;
    }
    goome_ui *u = goome_ui_new(g);
    viewer v = {.zoom = 0};
    if (argc > 1) open_file(&v, argv[1]);
    uint32_t *px = NULL;
    int pw = 0, ph = 0, running = 1, held = 0;
    double lx = 0, ly = 0;
    while (running) {
        goome_event ev;
        while (goome_poll(g, &ev, 16) > 0) {
            if (ev.type == GOOME_QUIT || ev.type == GOOME_CLOSE) running = 0;
            if (ev.type == GOOME_DROP) open_file(&v, goome_drop_path(g));
            if (ev.type == GOOME_SCROLL && v.loaded) v.zoom = v.shown * (ev.dy < 0 ? 1.25 : 0.8);
            if (ev.type == GOOME_DOWN && ev.y > 40) held = 1, lx = ev.x, ly = ev.y;
            if (ev.type == GOOME_MOTION && held) v.x += ev.x - lx, v.y += ev.y - ly, lx = ev.x, ly = ev.y;
            if (ev.type == GOOME_UP) held = 0;
            goome_ui_feed(u, &ev);
        }
        if (!running) break;
        goome_canvas *c = goome_ui_begin(u, 0);
        if (!c) continue;
        goome_monitor m;
        goome_monitors(g, &m, 1);
        double vw = m.area.w, vh = m.area.h - 72;
        int w = (int)(vw * c->scale + 0.5), h = (int)(vh * c->scale + 0.5);
        if (w < 1 || h < 1) w = h = 1;
        if (w != pw || h != ph) {
            free(px);
            px = malloc(sizeof(uint32_t) * (size_t)w * h);
            pw = w, ph = h;
        }
        if (!px) break;
        goome_canvas view = {.px = px, .w = w, .h = h, .stride = w, .scale = c->scale, .kx1 = w, .ky1 = h};
        for (int i = 0; i < w * h; i++) px[i] = ((i % w) / 12 + (i / w) / 12) & 1 ? 0xFF24272E : 0xFF1E2026;
        if (v.loaded) {
            double fit = vw / v.pic.w < vh / v.pic.h ? vw / v.pic.w : vh / v.pic.h;
            if (fit > 1) fit = 1;
            double z = v.zoom ? v.zoom : fit, iw = v.pic.w * z, ih = v.pic.h * z;
            v.shown = z;
            goome_draw_shadow(&view, (vw - iw) / 2 + v.x, (vh - ih) / 2 + v.y, iw, ih, 0, 16, 0x80000000);
            goome_draw_bitmap(&view, &v.pic, (vw - iw) / 2 + v.x, (vh - ih) / 2 + v.y, iw, ih, goome_radius(0), 0xFFFFFFFF);
        } else {
            const char *hint = "drop a picture on this window";
            goome_text(&view, NULL, 18, (vw - goome_text_measure(NULL, 18, hint, NULL)) / 2, vh / 2 - 12, hint, 0xFF8A93A3);
        }
        goome_bitmap shown = {.px = px, .w = w, .h = h, .stride = w};
        goome_ui_app(u, "Image viewer", GOOME_APP_MENUBAR | GOOME_APP_STATUSBAR | GOOME_APP_NO_PAD);
        if (goome_ui_menubar_begin(u)) {
            if (goome_ui_menu_begin(u, "View")) {
                if (goome_ui_menu_item(u, "Fit to window", "0", NULL)) v.zoom = 0, v.x = v.y = 0;
                if (goome_ui_menu_item(u, "Real size", "1", NULL)) v.zoom = 1, v.x = v.y = 0;
                goome_ui_menu_end(u);
            }
            goome_ui_menubar_end(u);
        }
        goome_ui_image(u, &shown, vw, vh, 0);
        char st[320];
        if (v.loaded) snprintf(st, sizeof(st), "%s   %dx%d   %.0f%%", v.name, v.pic.w, v.pic.h, v.shown * 100);
        else snprintf(st, sizeof(st), "nothing open, scroll zooms and dragging moves it");
        goome_ui_statusbar(u, st);
        goome_ui_app_end(u);
        goome_ui_end(u);
    }
    free(px);
    if (v.loaded) goome_bitmap_free(&v.pic);
    goome_ui_free(u);
    goome_close(g);
    return 0;
}
