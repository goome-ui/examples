#include <stdio.h>
#include <string.h>

#include "goome.h"

#define MAX_TODOS 128

typedef struct {
    char text[96];
    int done;
} todo;

static todo todos[MAX_TODOS];
static int ntodos;

static void add(const char *text, int done)
{
    if (ntodos == MAX_TODOS || !text[0]) return;
    snprintf(todos[ntodos].text, sizeof(todos[ntodos].text), "%s", text);
    todos[ntodos++].done = done;
}

static void load(const char *path)
{
    FILE *f = fopen(path, "r");
    char line[128];
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (line[0] && line[1] == ' ') add(line + 2, line[0] == 'x');
    }
    fclose(f);
}

static void save(const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) return;
    for (int i = 0; i < ntodos; i++) fprintf(f, "%c %s\n", todos[i].done ? 'x' : '-', todos[i].text);
    fclose(f);
}

int main(void)
{
    int err;
    goome *g = goome_open_app("Todo", 460, 520, &err);
    if (!g) {
        printf("couldnt open the todo window: %s\n", goome_strerror(err));
        return 1;
    }
    goome_ui *u = goome_ui_new(g);
    load("todo.txt");
    if (!ntodos) {
        add("water the plants", 1);
        add("reply to the landlord", 0);
        add("buy a new bike chain", 0);
        add("finish the goome example", 0);
    }
    static const char *const filters[] = {"All", "Open", "Done"};
    char typed[96] = "";
    int running = 1, filter = 0, submit = 0;
    while (running) {
        goome_event ev;
        while (goome_poll(g, &ev, 16) > 0) {
            if (ev.type == GOOME_QUIT || ev.type == GOOME_CLOSE) running = 0;
            if (ev.type == GOOME_KEY && ev.key == GOOME_K_ENTER) submit = 1;
            goome_ui_feed(u, &ev);
        }
        if (!running) break;
        if (!goome_ui_begin(u, 0)) continue;
        goome_ui_app(u, "Todo", GOOME_APP_STATUSBAR);
        goome_ui_width(u, 340);
        goome_ui_input(u, "##new", typed, sizeof(typed));
        goome_ui_same_line(u);
        if (goome_ui_button(u, "Add") || submit) add(typed, 0), typed[0] = 0, save("todo.txt");
        submit = 0;
        goome_ui_tabs(u, "filter", filters, 3, &filter);
        goome_ui_scroll_begin(u, "list", 0);
        int left = 0, gone = -1;
        for (int i = 0; i < ntodos; i++) {
            left += !todos[i].done;
            if ((filter == 1 && todos[i].done) || (filter == 2 && !todos[i].done)) continue;
            char id[16];
            snprintf(id, sizeof(id), "%d", i);
            goome_ui_push_id(u, id);
            if (goome_ui_checkbox(u, todos[i].text, &todos[i].done)) save("todo.txt");
            if (goome_ui_context_begin(u, "menu")) {
                if (goome_ui_menu_item(u, "Delete", NULL, NULL)) gone = i;
                if (goome_ui_menu_item(u, "Move to top", NULL, NULL) && i) {
                    todo t = todos[i];
                    memmove(&todos[1], &todos[0], sizeof(todo) * (size_t)i);
                    todos[0] = t;
                }
                goome_ui_context_end(u);
            }
            goome_ui_pop_id(u);
        }
        goome_ui_scroll_end(u);
        if (gone >= 0) {
            memmove(&todos[gone], &todos[gone + 1], sizeof(todo) * (size_t)(ntodos - gone - 1));
            ntodos--;
            save("todo.txt");
        }
        char st[64];
        snprintf(st, sizeof(st), "%d left, %d done   right click to delete", left, ntodos - left);
        goome_ui_statusbar(u, st);
        goome_ui_app_end(u);
        goome_ui_end(u);
    }
    goome_ui_free(u);
    goome_close(g);
}
