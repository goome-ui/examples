#include <stdio.h>

#include "goome.h"

static const char *const presets[] = {"Dark", "Light", "Midnight", "Paper", "Mint", "High contrast", "Rose"};
static const uint32_t accents[] = {0xFF4FB3F7, 0xFFFF7A59, 0xFF6FDC5F, 0xFFB57CFF, 0xFFFFB443};

static void dress(goome_ui *u, int own, int preset, int accent, int dark, double radius, double scale)
{
    goome_theme *t = goome_ui_theme(u);
    if (own) goome_ui_theme_make(t, accents[accent], dark, radius);
    else goome_ui_theme_preset(t, preset);
    goome_ui_scale(u, scale);
}

int main(void)
{
    int err;
    goome *g = goome_open_app("Themes", 620, 480, &err);
    if (!g) return fprintf(stderr, "no window: %s\n", goome_strerror(err)), 1;
    goome_ui *u = goome_ui_new(g);
    int running = 1, own = 0, preset = GOOME_PRESET_MIDNIGHT, accent = 0, dark = 1, check = 1, tab = 0;
    double radius = 10, scale = 1, volume = 0.6;
    char name[48] = "my theme";
    dress(u, own, preset, accent, dark, radius, scale);
    while (running) {
        goome_event ev;
        while (goome_poll(g, &ev, 16) > 0) {
            if (ev.type == GOOME_QUIT || ev.type == GOOME_CLOSE) running = 0;
            goome_ui_feed(u, &ev);
        }
        if (!running) break;
        if (!goome_ui_begin(u, 0)) continue;
        int changed = 0;
        goome_ui_app(u, "Themes", 0);
        goome_ui_heading(u, "Pick one");
        changed |= goome_ui_combo(u, "Preset", &preset, presets, 7);
        if (changed) own = 0;
        goome_ui_heading(u, "Or make your own");
        for (int i = 0; i < 5; i++) {
            char id[8];
            snprintf(id, sizeof(id), "c%d", i);
            if (i) goome_ui_same_line(u);
            goome_ui_push_color(u, GOOME_S_INPUT_BG, accents[i]);
            goome_ui_push_color(u, GOOME_S_HOVER, accents[i]);
            goome_ui_size(u, 36, 26);
            goome_ui_push_id(u, id);
            if (goome_ui_button(u, own && accent == i ? "x" : " ")) accent = i, own = 1, changed = 1;
            goome_ui_pop_id(u);
            goome_ui_pop(u, 2);
        }
        goome_ui_row(u, 2);
        if (goome_ui_toggle(u, "Dark", &dark)) own = 1, changed = 1;
        if (goome_ui_slider(u, "Corners", &radius, 0, 20, "%.0f px")) own = 1, changed = 1;
        changed |= goome_ui_slider(u, "Scale", &scale, 0.75, 1.75, "%.2fx");
        goome_ui_separator(u);
        static const char *const tabs[] = {"Preview", "More"};
        goome_ui_tabs(u, "tabs", tabs, 2, &tab);
        goome_ui_row(u, 2);
        goome_ui_checkbox(u, "A checkbox", &check);
        goome_ui_input(u, "##name", name, sizeof(name));
        goome_ui_progress(u, volume, NULL);
        goome_ui_slider(u, "Volume", &volume, 0, 1, "%.2f");
        goome_ui_button(u, "A button");
        goome_ui_app_end(u);
        goome_ui_end(u);
        if (changed) dress(u, own, preset, accent, dark, radius, scale);
    }
    goome_ui_free(u);
    goome_close(g);
    return 0;
}
