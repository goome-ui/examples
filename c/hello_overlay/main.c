#include <stdio.h>

#include "goome.h"

int main(void)
{
    int err;
    goome *g = goome_open(&(goome_opts){.title = "hello overlay"}, &err);
    if (!g) {
        fprintf(stderr, "cant open an overlay: %s\n", goome_strerror(err));
        return 1;
    }
    goome_ui *u = goome_ui_new(g);
    double x = 60, y = 60, w = 300, h = 0, volume = 0.6;
    int open = 1, clicks = 0, dark = 1;
    while (open) {
        goome_event ev;
        while (goome_poll(g, &ev, 16) > 0) {
            if (ev.type == GOOME_QUIT) open = 0;
            goome_ui_feed(u, &ev);
        }
        if (!open) break;
        if (!goome_ui_begin(u, 0)) continue;
        if (goome_ui_window(u, "hello", &x, &y, &w, &h, GOOME_WIN_MOVE | GOOME_WIN_CLOSE, &open)) {
            goome_ui_label(u, "this sits on top of everything");
            goome_ui_label(u, "clicks next to it go to the app under it");
            if (goome_ui_button(u, "Click me")) clicks++;
            goome_ui_same_line(u);
            goome_ui_label(u, "%d clicks", clicks);
            goome_ui_slider(u, "Volume", &volume, 0, 1, "%.2f");
            if (goome_ui_toggle(u, "Dark", &dark)) {
                goome_theme *t = goome_ui_theme(u);
                goome_font *f = t->font;
                if (dark) goome_ui_theme_dark(t);
                else goome_ui_theme_light(t);
                t->font = f;
            }
        }
        goome_ui_window_end(u);
        goome_ui_end(u);
    }
    goome_ui_free(u);
    goome_close(g);
    return 0;
}
