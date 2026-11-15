#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "goome.h"

typedef struct {
    const char *name, *cmd;
} app;

#if defined(_WIN32)
static const app apps[] = {
    {"Terminal", "start \"\" cmd"}, {"Files", "start \"\" explorer"}, {"Notepad", "start \"\" notepad"},
    {"Calculator", "start \"\" calc"}, {"Paint", "start \"\" mspaint"}, {"goome docs", "start \"\" https://goome-ui.github.io/docs/"},
};
#elif defined(__APPLE__)
static const app apps[] = {
    {"Terminal", "open -a Terminal"}, {"Files", "open ~"}, {"TextEdit", "open -a TextEdit"},
    {"Calculator", "open -a Calculator"}, {"Activity Monitor", "open -a 'Activity Monitor'"}, {"goome docs", "open https://goome-ui.github.io/docs/"},
};
#else
static const app apps[] = {
    {"Terminal", "x-terminal-emulator &"}, {"Files", "xdg-open ~ &"}, {"Text editor", "xdg-open ~/notes.txt &"},
    {"Calculator", "gnome-calculator &"}, {"System monitor", "gnome-system-monitor &"}, {"goome docs", "xdg-open https://goome-ui.github.io/docs/ &"},
};
#endif
#define NAPPS (int)(sizeof(apps) / sizeof(apps[0]))

static int matches(const char *name, const char *typed)
{
    for (; *typed; typed++) {
        while (*name && tolower((unsigned char)*name) != tolower((unsigned char)*typed)) name++;
        if (!*name++) return 0;
    }
    return 1;
}

int main(void)
{
    int err;
    goome *g = goome_open(&(goome_opts){.title = "launcher", .no_capture = 1}, &err);
    if (!g) {
        fprintf(stderr, "cant open an overlay: %s\n", goome_strerror(err));
        return 1;
    }
    if (goome_hotkey(g, "alt+space", 1) != GOOME_OK) printf("no global hotkeys here: %s\n", goome_error(g));
    goome_ui *u = goome_ui_new(g);
    char typed[64] = "";
    int open = 0, running = 1, pick = 0, launch = -1;
    while (running) {
        goome_event ev;
        while (goome_poll(g, &ev, open ? 16 : 1000) > 0) {
            if (ev.type == GOOME_QUIT) running = 0;
            if (ev.type == GOOME_HOTKEY) open = !open, typed[0] = 0, pick = 0;
            if (open && ev.type == GOOME_KEY && ev.key == GOOME_K_ESC) open = 0;
            if (open && ev.type == GOOME_KEY && ev.key == GOOME_K_ENTER) launch = pick;
            if (open && ev.type == GOOME_KEY && (ev.key == GOOME_K_UP || ev.key == GOOME_K_DOWN)) pick += ev.key == GOOME_K_UP ? -1 : 1;
            goome_ui_feed(u, &ev);
        }
        if (!running) break;
        int shown[NAPPS], n = 0;
        for (int i = 0; i < NAPPS; i++)
            if (matches(apps[i].name, typed)) shown[n++] = i;
        if (pick >= n) pick = n - 1;
        if (pick < 0) pick = 0;
        if (launch >= 0 && launch < n) {
            printf("starting %s\n", apps[shown[launch]].name);
            fflush(stdout);
            if (system(apps[shown[launch]].cmd) != 0) printf("that didnt start\n");
            open = 0;
        }
        launch = -1;
        goome_monitor m;
        if (goome_monitors(g, &m, 1) < 1) continue;
        if (!goome_ui_begin(u, 0)) continue;
        if (open) {
            double w = 420, h = 0, x = m.area.x + (m.area.w - w) / 2, y = m.area.y + m.area.h / 4.0;
            if (goome_ui_window(u, "launcher", &x, &y, &w, &h, GOOME_WIN_NO_TITLE, NULL)) {
                goome_ui_set_focus(u);
                goome_ui_input(u, "##typed", typed, sizeof(typed));
                for (int i = 0; i < n; i++)
                    if (goome_ui_selectable(u, apps[shown[i]].name, i == pick)) pick = i, launch = i;
                if (!n) goome_ui_label(u, "nothing called that");
            }
            goome_ui_window_end(u);
        }
        goome_ui_end(u);
    }
    goome_ui_free(u);
    goome_close(g);
    return 0;
}
