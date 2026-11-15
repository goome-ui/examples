#include <stdio.h>
#include <stdlib.h>

#include "goome.h"

#define DAYS 30
#define CHART_H 170

typedef struct {
    const char *who, *what;
    double total;
    int status;
} order;

static float visits[DAYS], sales[DAYS];
static uint32_t chart_px[1400 * CHART_H * 4];

static void make_data(void)
{
    unsigned seed = 7;
    for (int i = 0; i < DAYS; i++) {
        seed = seed * 1103515245u + 12345u;
        visits[i] = 400 + i * 14 + (float)(seed >> 16 & 255);
        sales[i] = visits[i] / 18 + (float)(seed >> 8 & 15);
    }
}

static goome_bitmap draw_chart(goome_font *font, double scale, double w)
{
    int pw = (int)(w * scale + 0.5), ph = (int)(CHART_H * scale + 0.5);
    if (pw > 1400) pw = 1400;
    for (int i = 0; i < pw * ph; i++) chart_px[i] = 0;
    goome_canvas c = {.px = chart_px, .w = pw, .h = ph, .stride = pw, .scale = scale, .kx1 = pw, .ky1 = ph};
    double top = 1000, x0 = 36, x1 = w - 30, y0 = 10, y1 = CHART_H - 20, pts[(DAYS + 2) * 2];
    for (int k = 0; k <= 4; k++) {
        double y = y1 - (y1 - y0) * k / 4;
        char label[16];
        snprintf(label, sizeof(label), "%.0f", top * k / 4);
        goome_draw_rect(&c, x0, y, x1 - x0, 1, 0x20FFFFFF);
        goome_text(&c, font, 10, 0, y - 6, label, 0xFF8A93A3);
    }
    for (int i = 0; i < DAYS; i++) pts[i * 2] = x0 + (x1 - x0) * i / (DAYS - 1), pts[i * 2 + 1] = y1 - (y1 - y0) * visits[i] / top;
    pts[DAYS * 2] = x1, pts[DAYS * 2 + 1] = y1, pts[DAYS * 2 + 2] = x0, pts[DAYS * 2 + 3] = y1;
    goome_draw_poly(&c, pts, DAYS + 2, 0x306FDC5F);
    for (int i = 1; i < DAYS; i++) goome_draw_line(&c, pts[i * 2 - 2], pts[i * 2 - 1], pts[i * 2], pts[i * 2 + 1], 2, 0xFF6FDC5F);
    goome_draw_circle(&c, pts[DAYS * 2 - 2], pts[DAYS * 2 - 1], 4, 0xFF6FDC5F);
    for (int i = 0; i < DAYS; i += 7) {
        char label[16];
        snprintf(label, sizeof(label), "day %d", i + 1);
        goome_text(&c, font, 10, pts[i * 2] - 8, y1 + 6, label, 0xFF8A93A3);
    }
    return (goome_bitmap){.px = chart_px, .w = pw, .h = ph, .stride = pw};
}

static uint32_t cards_px[1400 * 90 * 4];

static goome_bitmap draw_cards(goome_font *font, double scale, double w)
{
    static const char *const names[] = {"Visitors", "Sales", "Refunds", "Avg order"};
    static const char *const values[] = {"18,204", "1,093", "17", "$71.40"};
    static const char *const changes[] = {"+12% this month", "+4% this month", "-2% this month", "same as last month"};
    static const uint32_t colors[] = {0xFF6FDC5F, 0xFF6FDC5F, 0xFFFF6B6B, 0xFF8A93A3};
    int pw = (int)(w * scale + 0.5), ph = (int)(90 * scale + 0.5);
    if (pw > 1400) pw = 1400;
    for (int i = 0; i < pw * ph; i++) cards_px[i] = 0;
    goome_canvas c = {.px = cards_px, .w = pw, .h = ph, .stride = pw, .scale = scale, .kx1 = pw, .ky1 = ph};
    double cw = (w - 36) / 4;
    for (int i = 0; i < 4; i++) {
        double x = i * (cw + 12);
        goome_draw_round_rect(&c, x, 0, cw, 88, 12, 0xFF22252C);
        goome_text(&c, font, 13, x + 14, 10, names[i], 0xFF8A93A3);
        goome_text(&c, font, 26, x + 14, 28, values[i], 0xFFFFFFFF);
        goome_text(&c, font, 12, x + 14, 64, changes[i], colors[i]);
    }
    return (goome_bitmap){.px = cards_px, .w = pw, .h = ph, .stride = pw};
}

static int by_col, desc;

static int compare(const void *a, const void *b)
{
    const order *x = a, *y = b;
    int r = by_col == 2 ? (x->total > y->total) - (x->total < y->total) : by_col == 3 ? x->status - y->status : 0;
    return desc ? -r : r;
}

int main(void)
{
    int err;
    goome *g = goome_open_app("Dashboard", 900, 660, &err);
    if (!g) return printf("no window: %s\n", goome_strerror(err)), 1;
    goome_ui *u = goome_ui_new(g);
    make_data();
    static order orders[] = {
        {"Mia", "Desk lamp", 49.90, 2}, {"Jonas", "Keyboard", 129.00, 1}, {"Aiko", "Monitor arm", 89.50, 0},
        {"Liam", "USB hub", 24.99, 2}, {"Noor", "Webcam", 74.00, 1}, {"Sam", "Headphones", 199.00, 2},
    };
    static const char *const status[] = {"Waiting", "Shipped", "Delivered"};
    static const uint32_t status_color[] = {0xFFF0C24F, 0xFF5FB8F0, 0xFF6FDC5F};
    int running = 1;
    while (running) {
        goome_event ev;
        while (goome_poll(g, &ev, 16) > 0) {
            if (ev.type == GOOME_QUIT || ev.type == GOOME_CLOSE) running = 0;
            goome_ui_feed(u, &ev);
        }
        if (!running) break;
        goome_canvas *c = goome_ui_begin(u, 0);
        if (!c) continue;
        goome_monitor m;
        goome_monitors(g, &m, 1);
        goome_ui_app(u, "Dashboard", 0);
        goome_bitmap cards = draw_cards(goome_ui_theme(u)->font, c->scale, m.area.w - 28);
        goome_ui_image(u, &cards, m.area.w - 28, 90, 0);
        goome_ui_label(u, "Visitors, last 30 days");
        goome_bitmap chart = draw_chart(goome_ui_theme(u)->font, c->scale, m.area.w - 28);
        goome_ui_image(u, &chart, m.area.w - 28, CHART_H, 0);
        goome_ui_row(u, 2);
        goome_ui_plot_bars(u, "Sales per day", sales, DAYS, 0, 80, 70);
        goome_ui_plot_lines(u, "Visitors", visits, DAYS, 0, 1000, 70);
        if (goome_ui_table_begin(u, "orders", 4, 0, GOOME_TABLE_SORTABLE | GOOME_TABLE_STRIPES)) {
            goome_ui_table_column(u, "Customer", 0);
            goome_ui_table_column(u, "Item", 0);
            goome_ui_table_column(u, "Total", 100);
            goome_ui_table_column(u, "Status", 110);
            if (goome_ui_table_sort(u, &by_col, &desc)) qsort(orders, sizeof(orders) / sizeof(orders[0]), sizeof(order), compare);
            for (size_t i = 0; i < sizeof(orders) / sizeof(orders[0]); i++) {
                goome_ui_table_row(u);
                goome_ui_label(u, "%s", orders[i].who);
                goome_ui_table_next(u);
                goome_ui_label(u, "%s", orders[i].what);
                goome_ui_table_next(u);
                goome_ui_label(u, "$%.2f", orders[i].total);
                goome_ui_table_next(u);
                goome_ui_badge(u, status[orders[i].status], status_color[orders[i].status]);
            }
        }
        goome_ui_table_end(u);
        goome_ui_app_end(u);
        goome_ui_end(u);
    }
    goome_ui_free(u), goome_close(g);
    return 0;
}
