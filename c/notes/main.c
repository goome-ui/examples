#include <stdio.h>
#include <string.h>

#include "goome.h"

#define MAX_NOTES 64

typedef struct {
    char title[64];
    char body[4096];
} note;

static note notes[MAX_NOTES];
static int nnotes;

static int add(const char *title, const char *body)
{
    if (nnotes == MAX_NOTES) return nnotes - 1;
    snprintf(notes[nnotes].title, sizeof(notes[nnotes].title), "%s", title);
    snprintf(notes[nnotes].body, sizeof(notes[nnotes].body), "%s", body);
    return nnotes++;
}

static int save(const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) return 0;
    for (int i = 0; i < nnotes; i++) fprintf(f, "# %s\n%s\n\n", notes[i].title, notes[i].body);
    fclose(f);
    return 1;
}

int main(void)
{
    int err;
    goome *g = goome_open_app("Notes", 760, 480, &err);
    if (!g) {
        fprintf(stderr, "cant open a window: %s\n", goome_strerror(err));
        return 1;
    }
    goome_ui *u = goome_ui_new(g);
    add("Groceries", "oat milk\ncoffee beans\nlimes\nrice\ndish soap");
    add("Trip", "train at 8:10\nbring the good camera\nbook the place by the harbor");
    add("Ideas", "a clock that only shows the minutes\nbike rack for the balcony");
    int sel = 0, running = 1, wrap = 1, numbers = 0, changed = 0, about = 0;
    double side = 0.3;
    while (running) {
        goome_event ev;
        while (goome_poll(g, &ev, 16) > 0) {
            if (ev.type == GOOME_QUIT || ev.type == GOOME_CLOSE) running = 0;
            if (ev.type == GOOME_KEY && ev.mods == GOOME_CTRL && ev.key == 's') changed = !save("notes.md");
            if (ev.type == GOOME_KEY && ev.mods == GOOME_CTRL && ev.key == 'n') sel = add("Untitled", ""), changed = 1;
            goome_ui_feed(u, &ev);
        }
        if (!running) break;
        if (!goome_ui_begin(u, 0)) continue;
        goome_ui_app(u, "Notes", GOOME_APP_MENUBAR | GOOME_APP_STATUSBAR);
        if (goome_ui_menubar_begin(u)) {
            if (goome_ui_menu_begin(u, "File")) {
                if (goome_ui_menu_item(u, "New note", "Ctrl+N", NULL)) sel = add("Untitled", ""), changed = 1;
                if (goome_ui_menu_item(u, "Save", "Ctrl+S", NULL)) {
                    changed = !save("notes.md");
                    goome_ui_toast(u, changed ? "couldnt write notes.md" : "Saved to notes.md", 2000);
                }
                goome_ui_menu_separator(u);
                if (goome_ui_menu_item(u, "Quit", NULL, NULL)) running = 0;
                goome_ui_menu_end(u);
            }
            if (goome_ui_menu_begin(u, "View")) {
                goome_ui_menu_item(u, "Wrap lines", NULL, &wrap);
                goome_ui_menu_item(u, "Line numbers", NULL, &numbers);
                goome_ui_menu_end(u);
            }
            if (goome_ui_menu_begin(u, "Help")) {
                if (goome_ui_menu_item(u, "About", NULL, NULL)) about = 1;
                goome_ui_menu_end(u);
            }
            goome_ui_menubar_end(u);
        }
        if (goome_ui_split_begin(u, "main", 0, &side)) {
            for (int i = 0; i < nnotes; i++) {
                char row[80];
                snprintf(row, sizeof(row), "%.60s##%d", notes[i].title[0] ? notes[i].title : "Untitled", i);
                if (goome_ui_selectable(u, row, i == sel)) sel = i;
            }
            goome_ui_space(u, 6);
            if (goome_ui_button(u, "New note")) sel = add("Untitled", ""), changed = 1;
            goome_ui_split_next(u);
            note *n = &notes[sel];
            changed |= goome_ui_input(u, "Title", n->title, sizeof(n->title));
            changed |= goome_ui_text_edit(u, "body", n->body, sizeof(n->body), 0, (wrap ? GOOME_EDIT_WRAP : 0) | (numbers ? GOOME_EDIT_LINE_NUMBERS : 0));
        }
        goome_ui_split_end(u);
        if (goome_ui_modal_begin(u, "About Notes", &about)) {
            goome_ui_label(u, "a tiny notes app, made with goome %s", GOOME_VERSION);
            if (goome_ui_button(u, "Close")) goome_ui_popup_close(u);
            goome_ui_modal_end(u);
        }
        char st[96];
        snprintf(st, sizeof(st), "%d notes   %d characters   %s", nnotes, (int)strlen(notes[sel].body), changed ? "not saved" : "saved");
        goome_ui_statusbar(u, st);
        goome_ui_app_end(u);
        goome_ui_end(u);
    }
    goome_ui_free(u);
    goome_close(g);
    return 0;
}
