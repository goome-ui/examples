#include <stdio.h>
#include <string.h>

#include "goome.h"

typedef struct {
    char name[64];
    int start_up, updates, tray, language, light, sounds;
    double ui_size, volume;
    uint32_t accent;
} settings;

static const char *const languages[] = {"English", "Deutsch", "Español", "Français", "日本語"};

static void defaults(settings *s)
{
    memset(s, 0, sizeof(*s));
    snprintf(s->name, sizeof(s->name), "my laptop");
    s->updates = 1, s->tray = 1, s->sounds = 1, s->ui_size = 15, s->volume = 0.7, s->accent = 0xFF6FDC5F;
}

static int save(const settings *s, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) return 0;
    fprintf(f, "name=%s\nstart_up=%d\nupdates=%d\ntray=%d\nlanguage=%s\nlight=%d\nui_size=%.0f\naccent=%08X\nsounds=%d\nvolume=%.2f\n", s->name, s->start_up,
            s->updates, s->tray, languages[s->language], s->light, s->ui_size, (unsigned)s->accent, s->sounds, s->volume);
    fclose(f);
    return 1;
}

static void look(goome_ui *u, const settings *s)
{
    goome_theme *t = goome_ui_theme(u);
    goome_font *f = t->font;
    if (s->light) goome_ui_theme_light(t);
    else goome_ui_theme_dark(t);
    t->font = f, t->accent = s->accent, t->font_size = s->ui_size;
}

int main(void)
{
    int err;
    goome *g = goome_open_app("Settings", 560, 440, &err);
    if (!g) {
        fprintf(stderr, "cant open a window: %s\n", goome_strerror(err));
        return 1;
    }
    goome_ui *u = goome_ui_new(g);
    static const char *const tabs[] = {"General", "Look", "Sound", "About"};
    settings now, saved;
    defaults(&now);
    saved = now;
    int tab = 0, running = 1, ask_reset = 0;
    look(u, &now);
    while (running) {
        goome_event ev;
        while (goome_poll(g, &ev, 16) > 0) {
            if (ev.type == GOOME_QUIT || ev.type == GOOME_CLOSE) running = 0;
            goome_ui_feed(u, &ev);
        }
        if (!running) break;
        if (!goome_ui_begin(u, 0)) continue;
        goome_ui_app(u, "Settings", GOOME_APP_STATUSBAR);
        goome_ui_tabs(u, "tabs", tabs, 4, &tab);
        goome_ui_space(u, 8);
        int changed = 0;
        if (tab == 0) {
            goome_ui_input(u, "Computer name", now.name, sizeof(now.name));
            goome_ui_combo(u, "Language", &now.language, languages, 5);
            goome_ui_toggle(u, "Start when I log in", &now.start_up);
            goome_ui_toggle(u, "Check for updates", &now.updates);
            goome_ui_toggle(u, "Keep running in the tray", &now.tray);
        } else if (tab == 1) {
            goome_ui_label(u, "Theme");
            changed |= goome_ui_radio(u, "Dark", &now.light, 0);
            goome_ui_same_line(u);
            changed |= goome_ui_radio(u, "Light", &now.light, 1);
            changed |= goome_ui_slider(u, "Text size", &now.ui_size, 12, 20, "%.0f px");
            changed |= goome_ui_color(u, "Accent", &now.accent);
        } else if (tab == 2) {
            goome_ui_toggle(u, "Sounds", &now.sounds);
            goome_ui_slider(u, "Volume", &now.volume, 0, 1, "%.2f");
            goome_ui_progress(u, now.sounds ? now.volume : 0, now.sounds ? NULL : "muted");
        } else {
            goome_ui_heading(u, "Settings");
            goome_ui_wrapped(u, "a settings window like every app has, made with goome. Apply writes settings.ini next to where you started it.");
            goome_ui_label(u, "goome %s on %s", GOOME_VERSION, goome_backend_name(g));
        }
        if (changed) look(u, &now);
        goome_ui_space(u, 12);
        goome_ui_row(u, 3);
        if (goome_ui_button(u, "Apply")) {
            if (save(&now, "settings.ini")) saved = now, goome_ui_toast(u, "Saved to settings.ini", 2000);
            else goome_ui_toast(u, "couldnt write settings.ini", 3000);
        }
        if (goome_ui_button(u, "Undo changes")) now = saved, look(u, &now);
        if (goome_ui_button(u, "Reset all")) ask_reset = 1;
        if (goome_ui_modal_begin(u, "Reset everything?", &ask_reset)) {
            goome_ui_label(u, "all settings go back to how they started");
            goome_ui_row(u, 2);
            if (goome_ui_button(u, "Reset")) defaults(&now), look(u, &now), goome_ui_popup_close(u);
            if (goome_ui_button(u, "Cancel")) goome_ui_popup_close(u);
            goome_ui_modal_end(u);
        }
        goome_ui_statusbar(u, memcmp(&now, &saved, sizeof(now)) ? "not applied yet" : "everything is saved");
        goome_ui_app_end(u);
        goome_ui_end(u);
    }
    goome_ui_free(u);
    goome_close(g);
}
