#include <stdio.h>
#include <string.h>

#include "goome.h"

typedef struct {
    char song[64], artist[64], alert[96];
    int followers, goal;
    long alert_at;
} stream;

static void handle(stream *s, const char *line)
{
    if (!strncmp(line, "follow ", 7)) {
        s->followers++;
        snprintf(s->alert, sizeof(s->alert), "%.60s just followed!", line + 7);
        s->alert_at = goome_now_ms();
    } else if (!strncmp(line, "sub ", 4)) {
        snprintf(s->alert, sizeof(s->alert), "%.60s subscribed, thank you!", line + 4);
        s->alert_at = goome_now_ms();
    } else if (!strncmp(line, "song ", 5)) {
        const char *dash = strstr(line + 5, " - ");
        snprintf(s->song, sizeof(s->song), "%.*s", dash ? (int)(dash - line - 5) : 63, line + 5);
        snprintf(s->artist, sizeof(s->artist), "%s", dash ? dash + 3 : "");
    }
}

static void read_new_lines(stream *s, const char *path, long *seen)
{
    FILE *f = fopen(path, "r");
    char line[160];
    if (!f) return;
    if (fseek(f, *seen, SEEK_SET) == 0)
        while (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\r\n")] = 0;
            handle(s, line);
        }
    *seen = ftell(f);
    fclose(f);
}

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "stream.txt";
    int err;
    goome *g = goome_open(&(goome_opts){.title = "stream widget", .no_capture = 1, .state_dir = "stream-widget"}, &err);
    if (!g) {
        fprintf(stderr, "%s\n", goome_strerror(err));
        return 1;
    }
    goome_font *font = goome_font_system(NULL);
    printf("add lines like \"follow Sam\", \"sub Sam\" or \"song Title - Artist\" to %s\n", path);
    stream s = {.song = "Nothing yet", .artist = "add a song line", .followers = 486, .goal = 500};
    long seen = 0;
    read_new_lines(&s, path, &seen);
    s.alert[0] = 0;
    int running = 1;
    goome_pacer pace = {0};
    while (running) {
        goome_event ev;
        while (goome_poll(g, &ev, goome_pace(&pace, 30)) > 0) {
            if (ev.type == GOOME_QUIT) running = 0;
            if (ev.type == GOOME_CONTROL) handle(&s, ev.msg);
        }
        if (!running) break;
        read_new_lines(&s, path, &seen);
        goome_monitor m;
        if (goome_monitors(g, &m, 1) < 1) continue;
        goome_canvas *c = goome_begin(g, 0);
        if (!c) continue;
        goome_clear(c);
        double x = m.area.x + 24, y = m.area.y + m.area.h - 110;
        goome_draw_round_rect(c, x, y, 300, 86, 14, 0xE01B1D22);
        goome_draw_rrect_grad(c, x + 12, y + 12, 62, 62, goome_radius(10), 0xFF6FDC5F, 0xFF2F8F7F, 0);
        goome_draw_triangle(c, x + 36, y + 30, x + 36, y + 56, x + 56, y + 43, 0xFFFFFFFF);
        goome_text(c, font, 11, x + 88, y + 14, "NOW PLAYING", 0xFF6FDC5F);
        goome_text(c, font, 16, x + 88, y + 30, s.song, 0xFFFFFFFF);
        goome_text(c, font, 13, x + 88, y + 52, s.artist, 0xFF9AA3B2);

        double gx = m.area.x + m.area.w - 324, gy = m.area.y + 24;
        char line[96];
        snprintf(line, sizeof(line), "Follower goal  %d / %d", s.followers, s.goal);
        goome_draw_round_rect(c, gx, gy, 300, 52, 12, 0xE01B1D22);
        goome_text(c, font, 13, gx + 14, gy + 8, line, 0xFFFFFFFF);
        double frac = s.followers >= s.goal ? 1 : (double)s.followers / s.goal;
        goome_draw_round_rect(c, gx + 14, gy + 32, 272, 8, 4, 0xFF2A2E36);
        goome_draw_round_rect(c, gx + 14, gy + 32, 272 * frac, 8, 4, 0xFF6FDC5F);

        long age = goome_now_ms() - s.alert_at;
        if (s.alert[0] && age < 5000) {
            double a = age < 300 ? age / 300.0 : age > 4500 ? (5000 - age) / 500.0 : 1;
            double w = goome_text_measure(font, 20, s.alert, NULL) + 48, ax = m.area.x + (m.area.w - w) / 2, ay = m.area.y + 40 - (1 - a) * 30;
            uint32_t alpha = (uint32_t)(a * 255) << 24;
            goome_draw_round_rect(c, ax, ay, w, 52, 26, alpha | 0x6FDC5F);
            goome_text(c, font, 20, ax + 24, ay + 13, s.alert, alpha | 0x10241A);
        }
        goome_end(g, c, NULL, 0);
    }
    goome_font_free(font);
    goome_close(g);
    return 0;
}
