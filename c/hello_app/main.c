#include <stdio.h>

#include "goome.h"

int main(void)
{
    int err;
    goome *g = goome_open_app("Hello app", 520, 340, &err);
    if (!g) {
        fprintf(stderr, "cant open a window: %s\n", goome_strerror(err));
        return 1;
    }
    goome_ui *u = goome_ui_new(g);
    char name[64] = "";
    int running = 1, greeted = 0;
    while (running) {
        goome_event ev;
        while (goome_poll(g, &ev, 16) > 0) {
            if (ev.type == GOOME_QUIT || ev.type == GOOME_CLOSE) running = 0;
            goome_ui_feed(u, &ev);
        }
        if (!running) break;
        if (!goome_ui_begin(u, 0)) continue;
        goome_ui_app(u, "Hello app", 0);
        goome_ui_heading(u, "Hello!");
        goome_ui_wrapped(u, "a normal window with a title bar, you can move it, resize it and minimize it like any other app.");
        goome_ui_space(u, 8);
        goome_ui_input(u, "Your name", name, sizeof(name));
        if (goome_ui_button(u, "Say hi")) greeted = 1;
        if (greeted) goome_ui_label(u, "hi %s, nice to meet you", name[0] ? name : "stranger");
        goome_ui_app_end(u);
        goome_ui_end(u);
    }
    goome_ui_free(u);
    goome_close(g);
    return 0;
}
