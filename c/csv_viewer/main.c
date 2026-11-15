#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "goome.h"

#define MAX_COLS 8
#define CELL 48

typedef struct {
    char cell[MAX_COLS][CELL];
} row;

static row *rows;
static int nrows, ncols, sort_col, sort_desc;
static char head[MAX_COLS][CELL];

static void split(const char *line, char out[MAX_COLS][CELL], int *n)
{
    int col = 0, at = 0, quoted = 0;
    memset(out, 0, sizeof(char) * MAX_COLS * CELL);
    for (; *line && *line != '\n' && *line != '\r' && col < MAX_COLS; line++) {
        if (*line == '"') quoted = !quoted;
        else if (*line == ',' && !quoted) col++, at = 0;
        else if (at < CELL - 1) out[col][at++] = *line;
    }
    if (n) *n = col + 1 < MAX_COLS ? col + 1 : MAX_COLS;
}

static void add_row(const char *line)
{
    static int cap;
    if (nrows == cap) {
        cap = cap ? cap * 2 : 256;
        rows = realloc(rows, sizeof(row) * (size_t)cap);
        if (!rows) exit(1);
    }
    split(line, rows[nrows++].cell, NULL);
}

static int load(const char *path)
{
    FILE *f = fopen(path, "r");
    char line[1024];
    if (!f) return 0;
    if (fgets(line, sizeof(line), f)) split(line, head, &ncols);
    while (fgets(line, sizeof(line), f)) add_row(line);
    fclose(f);
    return 1;
}

static void make_sample(void)
{
    static const char *const cities[] = {"Oslo", "Lisbon", "Osaka", "Lima", "Cairo", "Perth", "Quebec", "Tallinn"};
    static const char *const kinds[] = {"Rain", "Sun", "Snow", "Wind", "Fog"};
    split("City,Day,Weather,High,Low", head, &ncols);
    unsigned seed = 3;
    for (int i = 0; i < 2000; i++) {
        char line[128];
        seed = seed * 1103515245u + 12345u;
        int hi = (int)(seed >> 16 & 31) + 2;
        snprintf(line, sizeof(line), "%s,%d,%s,%d,%d", cities[i % 8], i / 8 + 1, kinds[(seed >> 8) % 5], hi, hi - (int)(seed >> 4 & 7) - 1);
        add_row(line);
    }
}

static int compare(const void *a, const void *b)
{
    const char *x = ((const row *)a)->cell[sort_col], *y = ((const row *)b)->cell[sort_col];
    char *ex, *ey;
    double nx = strtod(x, &ex), ny = strtod(y, &ey);
    int r = ex != x && ey != y ? (nx > ny) - (nx < ny) : strcmp(x, y);
    return sort_desc ? -r : r;
}

int main(int argc, char **argv)
{
    if (argc > 1 && !load(argv[1])) {
        fprintf(stderr, "cant read %s\n", argv[1]);
        return 1;
    }
    if (argc < 2) make_sample();
    int err;
    goome *g = goome_open_app(argc > 1 ? argv[1] : "CSV viewer", 720, 480, &err);
    if (!g) {
        fprintf(stderr, "cant open a window: %s\n", goome_strerror(err));
        return 1;
    }
    goome_ui *u = goome_ui_new(g);
    char filter[64] = "";
    int running = 1, *shown = malloc(sizeof(int) * (size_t)(nrows + 1));
    while (running && shown) {
        goome_event ev;
        while (goome_poll(g, &ev, 16) > 0) {
            if (ev.type == GOOME_QUIT || ev.type == GOOME_CLOSE) running = 0;
            goome_ui_feed(u, &ev);
        }
        if (!running) break;
        if (!goome_ui_begin(u, 0)) continue;
        goome_ui_app(u, "CSV viewer", GOOME_APP_STATUSBAR);
        goome_ui_input(u, "Search", filter, sizeof(filter));
        int n = 0;
        for (int i = 0; i < nrows; i++) {
            int hit = !filter[0];
            for (int k = 0; k < ncols && !hit; k++) hit = strstr(rows[i].cell[k], filter) != NULL;
            if (hit) shown[n++] = i;
        }
        if (goome_ui_table_begin(u, "rows", ncols, 0, GOOME_TABLE_SORTABLE | GOOME_TABLE_RESIZABLE | GOOME_TABLE_STRIPES)) {
            for (int k = 0; k < ncols; k++) goome_ui_table_column(u, head[k], 0);
            if (goome_ui_table_sort(u, &sort_col, &sort_desc)) qsort(rows, (size_t)nrows, sizeof(row), compare);
            int first, last;
            goome_ui_list_clip(u, n, 28, &first, &last);
            for (int i = first; i < last; i++) {
                goome_ui_table_row(u);
                for (int k = 0; k < ncols; k++) {
                    if (k) goome_ui_table_next(u);
                    goome_ui_label(u, "%s", rows[shown[i]].cell[k]);
                }
            }
        }
        goome_ui_table_end(u);
        char st[64];
        snprintf(st, sizeof(st), "%d of %d rows", n, nrows);
        goome_ui_statusbar(u, st);
        goome_ui_app_end(u);
        goome_ui_end(u);
    }
    free(shown);
    free(rows);
    goome_ui_free(u);
    goome_close(g);
    return 0;
}
