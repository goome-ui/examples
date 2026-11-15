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

#define MAX_NOTES 64
#define TEXT_CAP 4096
#define HEADER 28

enum { HOTKEY_EDIT = 1, HOTKEY_NEW, HOTKEY_HIDE };

typedef struct {
    double x, y, w, h;
    int color, ghost, alive;
    char text[TEXT_CAP];
} note;

typedef struct {
    uint32_t paper, header, ink;
} paper;

static const paper papers[] = {
    {0xFFFFF1A8, 0xFFFFE070, 0xFF3A3420},
    {0xFFFFD3E2, 0xFFFFB3CC, 0xFF41232F},
    {0xFFCDEBFF, 0xFFA9DBFF, 0xFF1F3242},
    {0xFFD4F5C9, 0xFFB5EAA4, 0xFF23361E},
    {0xFFE6DAFF, 0xFFD2BFFF, 0xFF2D2442},
    {0xFF2B2F36, 0xFF3A3F48, 0xFFE8EAEE},
};
#define PAPERS ((int)(sizeof(papers) / sizeof(*papers)))

static note notes[MAX_NOTES];

static void config_dir(char *out, int cap)
{
#ifdef _WIN32
    const char *base = getenv("APPDATA");
    snprintf(out, cap, "%s\\goome-sticky", base ? base : ".");
    _mkdir(out);
#elif defined(__APPLE__)
    const char *home = getenv("HOME");
    snprintf(out, cap, "%s/Library/Application Support/goome-sticky", home ? home : ".");
    mkdir(out, 0700);
#else
    const char *xdg = getenv("XDG_CONFIG_HOME"), *home = getenv("HOME");
    if (xdg && *xdg) snprintf(out, cap, "%s/goome-sticky", xdg);
    else snprintf(out, cap, "%s/.config/goome-sticky", home ? home : ".");
    mkdir(out, 0700);
#endif
}

static int add_note(double x, double y, int color, const char *text)
{
    for (int i = 0; i < MAX_NOTES; i++) {
        if (notes[i].alive) continue;
        note *n = &notes[i];
        memset(n, 0, sizeof(*n));
        n->x = x, n->y = y, n->w = 240, n->h = 220, n->color = color % PAPERS, n->alive = 1;
        snprintf(n->text, sizeof(n->text), "%s", text);
        return i;
    }
    return -1;
}

static int save_notes(const char *path)
{
    char tmp[1200];
    snprintf(tmp, sizeof(tmp), "%s.new", path);
    FILE *f = fopen(tmp, "w");
    if (!f) return 0;
    fprintf(f, "goome-sticky 1\n");
    for (int i = 0; i < MAX_NOTES; i++) {
        note *n = &notes[i];
        if (!n->alive) continue;
        fprintf(f, "note %.0f %.0f %.0f %.0f %d %d\n", n->x, n->y, n->w, n->h, n->color, n->ghost);
        fputs("text ", f);
        for (const char *p = n->text; *p; p++) {
            if (*p == '\\') fputs("\\\\", f);
            else if (*p == '\n') fputs("\\n", f);
            else if (*p != '\r') fputc(*p, f);
        }
        fputc('\n', f);
    }
    int ok = fclose(f) == 0;
    if (!ok) return 0;
    remove(path);
    return rename(tmp, path) == 0;
}

static int load_notes(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    static char line[TEXT_CAP * 2 + 16];
    int cur = -1;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        double x, y, w, h;
        int color, ghost;
        if (sscanf(line, "note %lf %lf %lf %lf %d %d", &x, &y, &w, &h, &color, &ghost) == 6) {
            cur = add_note(x, y, color < 0 ? 0 : color, "");
            if (cur >= 0) notes[cur].w = w < 160 ? 160 : w, notes[cur].h = h < 120 ? 120 : h, notes[cur].ghost = !!ghost;
        } else if (!strncmp(line, "text ", 5) && cur >= 0) {
            char *o = notes[cur].text, *end = o + TEXT_CAP - 1;
            for (const char *p = line + 5; *p && o < end; p++) {
                if (*p == '\\' && p[1]) {
                    p++;
                    *o++ = *p == 'n' ? '\n' : *p;
                } else *o++ = *p;
            }
            *o = 0;
        }
    }
    fclose(f);
    return 1;
}

static void wrap_text(goome_canvas *c, goome_font *f, double size, double x, double y, double w, double bottom, const char *s, uint32_t col)
{
    double lh = (int)(goome_text_line_height(f, size) + 3.5);
    char line[512];
    while (*s && y < bottom) {
        int len = 0, fit = 0;
        while (s[len] && s[len] != '\n' && len < (int)sizeof(line) - 1) {
            memcpy(line, s, (size_t)len + 1);
            line[len + 1] = 0;
            if (goome_text_measure(f, size, line, NULL) > w) break;
            len++;
            if (s[len] == ' ' || s[len] == '\n' || !s[len]) fit = len;
        }
        if (!fit) fit = len ? len : 1;
        memcpy(line, s, (size_t)fit);
        line[fit] = 0;
        goome_text(c, f, size, x, y, line, col);
        y += lh;
        s += fit;
        if (*s == ' ' || *s == '\n') s++;
    }
}

static void pin_icon(goome_canvas *c, double cx, double cy, int ghost, uint32_t col)
{
    if (ghost) {
        goome_draw_ring(c, cx, cy, 5.5, 1.5, col);
        goome_draw_line(c, cx - 7, cy + 7, cx + 7, cy - 7, 1.5, col);
    } else {
        goome_draw_circle(c, cx, cy - 2, 4.5, col);
        goome_draw_line(c, cx, cy + 2, cx, cy + 8, 1.5, col);
    }
}

static void draw_ghost(goome_canvas *c, goome_font *f, const note *n, double alpha)
{
    const paper *p = &papers[n->color];
    uint32_t a = (uint32_t)(alpha * 255) << 24;
    goome_draw_shadow(c, n->x, n->y + 4, n->w, n->h, 10, 14, (uint32_t)(alpha * 0x55) << 24);
    goome_draw_rrect(c, n->x, n->y, n->w, n->h, goome_radius(10), a | (p->paper & 0xFFFFFF));
    goome_draw_rrect(c, n->x, n->y, n->w, HEADER, (goome_radii){10, 10, 0, 0}, a | (p->header & 0xFFFFFF));
    pin_icon(c, n->x + n->w - 16, n->y + HEADER / 2.0, 1, a | (p->ink & 0xFFFFFF));
    goome_clip_push(c, n->x, n->y, n->w, n->h);
    wrap_text(c, f, 15, n->x + 8, n->y + HEADER + 19, n->w - 24, n->y + n->h - 8, n->text, a | (p->ink & 0xFFFFFF));
    goome_clip_pop(c);
}

static int note_window(goome_ui *u, note *n, int idx, int *changed)
{
    const paper *p = &papers[n->color];
    char id[32];
    snprintf(id, sizeof(id), "##note%d", idx);
    goome_ui_push_color(u, GOOME_S_PANEL, p->paper);
    goome_ui_push_color(u, GOOME_S_BORDER, 0x22000000);
    goome_ui_push_color(u, GOOME_S_TEXT, p->ink);
    goome_ui_push_color(u, GOOME_S_TEXT_DIM, (p->ink & 0xFFFFFF) | 0x99000000);
    goome_ui_push_color(u, GOOME_S_INPUT_BG, 0x00000000);
    goome_ui_push_color(u, GOOME_S_CARET, p->ink);
    goome_ui_push_color(u, GOOME_S_SELECT, (p->ink & 0xFFFFFF) | 0x33000000);
    goome_ui_push_color(u, GOOME_S_FOCUS, 0x00000000);
    goome_ui_push_color(u, GOOME_S_ACCENT, p->header);
    goome_ui_push_num(u, GOOME_S_PAD, 0);
    int keep = 1;
    double ox = n->x, oy = n->y, ow = n->w, oh = n->h;
    if (goome_ui_window(u, id, &n->x, &n->y, &n->w, &n->h, GOOME_WIN_NO_TITLE | GOOME_WIN_RESIZE, NULL)) {
        goome_ui_item hd;
        goome_ui_custom(u, "header", 0, HEADER, GOOME_ITEM_DRAG_X | GOOME_ITEM_DRAG_Y, &hd);
        goome_canvas *c = goome_ui_canvas(u);
        goome_draw_rrect(c, hd.x, hd.y, hd.w, hd.h, (goome_radii){10, 10, 0, 0}, p->header);
        double bx_del = hd.x + hd.w - 16, bx_pin = bx_del - 26, bx_col = bx_pin - 26, cy = hd.y + hd.h / 2;
        int over = -1;
        if (hd.hovered && hd.my >= hd.y && hd.my < hd.y + hd.h) {
            if (hd.mx > bx_del - 12) over = 0;
            else if (hd.mx > bx_pin - 12 && hd.mx < bx_pin + 12) over = 1;
            else if (hd.mx > bx_col - 12 && hd.mx < bx_col + 12) over = 2;
        }
        uint32_t ink = p->ink, soft = (p->ink & 0xFFFFFF) | 0x66000000;
        for (int k = 0; k < 3; k++)
            if (over == k) goome_draw_circle(c, k == 0 ? bx_del : k == 1 ? bx_pin : bx_col, cy, 11, (p->ink & 0xFFFFFF) | 0x1A000000);
        goome_draw_line(c, bx_del - 4, cy - 4, bx_del + 4, cy + 4, 1.6, over == 0 ? ink : soft);
        goome_draw_line(c, bx_del + 4, cy - 4, bx_del - 4, cy + 4, 1.6, over == 0 ? ink : soft);
        pin_icon(c, bx_pin, cy, n->ghost, over == 1 ? ink : soft);
        goome_draw_circle(c, bx_col, cy, 6, papers[(n->color + 1) % PAPERS].paper);
        goome_draw_ring(c, bx_col, cy, 6, 1.2, over == 2 ? ink : soft);
        for (int k = 0; k < 3; k++) goome_draw_circle(c, hd.x + 14 + k * 6, cy, 1.5, soft);
        if (hd.held && (hd.dx || hd.dy)) n->x += hd.dx, n->y += hd.dy;
        if (hd.clicked && hd.drag_x == 0 && hd.drag_y == 0) {
            if (over == 0) keep = 0;
            if (over == 1) n->ghost = !n->ghost;
            if (over == 2) n->color = (n->color + 1) % PAPERS;
            if (over >= 0) *changed = 1;
        }
        goome_ui_space(u, 4);
        goome_ui_push_num(u, GOOME_S_PAD, 10);
        goome_ui_push_num(u, GOOME_S_BORDER_WIDTH, 0);
        goome_ui_push_color(u, GOOME_S_BORDER, 0x00000000);
        if (goome_ui_text_edit(u, "body", n->text, TEXT_CAP, n->h - HEADER - 14, GOOME_EDIT_WRAP)) *changed = 1;
        goome_ui_pop(u, 3);
    }
    goome_ui_window_end(u);
    goome_ui_pop(u, 10);
    if (n->x != ox || n->y != oy || n->w != ow || n->h != oh) *changed = 1;
    return keep;
}

int main(int argc, char **argv)
{
    char dir[1024], path[1100];
    config_dir(dir, sizeof(dir));
    if (argc >= 3 && !strcmp(argv[1], "--send")) {
        int rc = goome_send(dir, argv[2]);
        if (rc != GOOME_OK) fprintf(stderr, "sticky: %s, is it running?\n", goome_strerror(rc));
        return rc != GOOME_OK;
    }
    int err;
    goome *g = goome_open(&(goome_opts){.title = "sticky", .no_capture = 1, .state_dir = dir}, &err);
    if (!g) {
        fprintf(stderr, "sticky: cant open an overlay here, %s\n", goome_strerror(err));
        return 1;
    }
    goome_ui *u = goome_ui_new(g);
    goome_theme *th = goome_ui_theme(u);
    goome_ui_theme_make(th, 0xFFFFC94D, 1, 10);
    goome_font *font = th->font;
    th->font_size = 15;
    int test = !strcmp(goome_backend_name(g), "test");
    snprintf(path, sizeof(path), test ? "notes.txt" : "%s/notes.txt", dir);
    goome_hotkey(g, "alt+shift+n", HOTKEY_EDIT);
    goome_hotkey(g, "alt+shift+m", HOTKEY_NEW);
    goome_hotkey(g, "alt+shift+b", HOTKEY_HIDE);

    goome_monitor m;
    if (goome_monitors(g, &m, 1) < 1) m.area = (goome_rect){0, 0, 1280, 720};
    int editing = 0, hidden = 0, running = 1, dirty = 1, unsaved = 0, spawn = 0;
    if (!load_notes(path)) {
        editing = 1;
        int a = add_note(m.area.x + 80, m.area.y + 90, 0,
                         "hi! this is a sticky note.\n\ntype on it, drag it by the top, resize it from the corner.\n\nthe pin makes a note click-through, so it floats over your stuff and clicks go to whatever is under it.");
        int b = add_note(m.area.x + 350, m.area.y + 120, 2,
                         "alt+shift+n  edit mode (shows click-through notes too)\nalt+shift+m  new note\nalt+shift+b  hide all\n\nnotes save by themselves.");
        if (b >= 0) notes[b].h = 190;
        if (a >= 0) notes[a].h = 250;
        unsaved = 1;
    }
    long last_change = goome_now_ms();
    double dx = m.area.x + m.area.w / 2.0 - 260, dy = m.area.y + m.area.h - 90, dw = 520, dh = 0;
    goome_pacer pace = {0};
    while (running) {
        goome_event ev;
        int wait = test || goome_ui_animating(u) || dirty ? goome_pace(&pace, 60) : unsaved ? 300 : 1000;
        while (goome_poll(g, &ev, wait) > 0) {
            wait = 0;
            dirty = 1;
            int cmd = 0;
            if (ev.type == GOOME_QUIT) running = 0;
            if (ev.type == GOOME_HOTKEY) cmd = ev.id;
            if (ev.type == GOOME_CONTROL) cmd = !strcmp(ev.msg, "edit") ? HOTKEY_EDIT : !strcmp(ev.msg, "new") ? HOTKEY_NEW : !strcmp(ev.msg, "hide") ? HOTKEY_HIDE : 0;
            if (ev.type == GOOME_CONTROL && !strcmp(ev.msg, "quit")) running = 0;
            if (cmd == HOTKEY_EDIT) editing = !editing, hidden = 0;
            if (cmd == HOTKEY_HIDE) hidden = !hidden, editing = 0;
            if (cmd == HOTKEY_NEW) {
                spawn = (spawn + 1) % 8;
                add_note(m.area.x + m.area.w / 2.0 - 120 + spawn * 24, m.area.y + m.area.h / 3.0 + spawn * 24, spawn, "");
                hidden = 0, unsaved = 1, last_change = goome_now_ms();
            }
            if (ev.type == GOOME_MONITORS && goome_monitors(g, &m, 1) < 1) m.area = (goome_rect){0, 0, 1280, 720};
            goome_ui_feed(u, &ev);
        }
        if (!running) break;
        long now = goome_now_ms();
        if (unsaved && now - last_change > 800) unsaved = !save_notes(path), last_change = now;
        if (!dirty && !test && !goome_ui_animating(u)) continue;
        dirty = 0;
        goome_canvas *c = goome_ui_begin(u, 0);
        if (!c) continue;
        if (!hidden) {
            for (int i = 0; i < MAX_NOTES; i++)
                if (notes[i].alive && notes[i].ghost && !editing) draw_ghost(c, font, &notes[i], 0.78);
            for (int i = 0; i < MAX_NOTES; i++) {
                if (!notes[i].alive || (notes[i].ghost && !editing)) continue;
                int changed = 0;
                if (!note_window(u, &notes[i], i, &changed)) notes[i].alive = 0, changed = 1;
                if (changed) unsaved = 1, last_change = now;
            }
        }
        if (editing) {
            if (goome_ui_window(u, "##dock", &dx, &dy, &dw, &dh, GOOME_WIN_NO_TITLE | GOOME_WIN_MOVE, NULL)) {
                goome_ui_row(u, 4);
                if (goome_ui_button(u, "New note")) {
                    spawn = (spawn + 1) % 8;
                    add_note(m.area.x + m.area.w / 2.0 - 120 + spawn * 24, m.area.y + m.area.h / 3.0 + spawn * 24, spawn, "");
                    unsaved = 1, last_change = now;
                }
                if (goome_ui_button(u, "Hide all")) hidden = 1, editing = 0;
                if (goome_ui_button(u, "Done")) editing = 0;
                if (goome_ui_button(u, "Quit")) running = 0;
            }
            goome_ui_window_end(u);
        }
        goome_ui_end(u);
    }
    if (unsaved) save_notes(path);
    goome_ui_free(u);
    goome_close(g);
    return 0;
}
