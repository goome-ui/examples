#include <stdio.h>

#include "goome.h"

int main(void)
{
    int err;
    goome *g = goome_open(&(goome_opts){.title = "region shot", .hide_self = 1}, &err);
    if (!g) {
        fprintf(stderr, "cant open an overlay: %s\n", goome_strerror(err));
        return 1;
    }
    goome_font *font = goome_font_system(NULL);
    if (goome_hotkey(g, "alt+s", 1) != GOOME_OK) printf("no global hotkeys here: %s\n", goome_error(g));
    int picking = 0, dragging = 0, running = 1, saved = 0;
    double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    char note[96] = "";
    long note_until = 0;
    while (running) {
        goome_event ev;
        while (goome_poll(g, &ev, 33) > 0) {
            if (ev.type == GOOME_QUIT) running = 0;
            if (ev.type == GOOME_HOTKEY) picking = !picking, dragging = 0, goome_keyboard(g, picking);
            if (!picking) continue;
            if (ev.type == GOOME_KEY && ev.key == GOOME_K_ESC) picking = 0, goome_keyboard(g, 0);
            if (ev.type == GOOME_DOWN) dragging = 1, x0 = x1 = ev.x, y0 = y1 = ev.y;
            if (ev.type == GOOME_MOTION && dragging) x1 = ev.x, y1 = ev.y;
            if (ev.type == GOOME_UP && dragging) {
                goome_rect r = {(int)(x0 < x1 ? x0 : x1), (int)(y0 < y1 ? y0 : y1), (int)(x0 < x1 ? x1 - x0 : x0 - x1), (int)(y0 < y1 ? y1 - y0 : y0 - y1)};
                goome_image img;
                char path[64];
                snprintf(path, sizeof(path), "shot-%d.ppm", ++saved);
                if (r.w < 2 || r.h < 2) snprintf(note, sizeof(note), "too small, drag a bigger box");
                else if (goome_capture(g, 0, &r, &img) != GOOME_OK) snprintf(note, sizeof(note), "couldnt grab it: %s", goome_error(g));
                else if (goome_save_ppm(path, img.px, img.w, img.h, img.stride) != GOOME_OK) snprintf(note, sizeof(note), "couldnt write %s", path);
                else snprintf(note, sizeof(note), "saved %s (%dx%d)", path, r.w, r.h);
                printf("%s\n", note);
                fflush(stdout);
                note_until = goome_now_ms() + 2500;
                dragging = 0;
            }
        }
        if (!running) break;
        goome_monitor m;
        if (goome_monitors(g, &m, 1) < 1) continue;
        goome_canvas *c = goome_begin(g, 0);
        if (!c) continue;
        goome_clear(c);
        goome_rect all = m.area;
        if (picking) {
            double l = x0 < x1 ? x0 : x1, t = y0 < y1 ? y0 : y1, w = x0 < x1 ? x1 - x0 : x0 - x1, h = y0 < y1 ? y1 - y0 : y0 - y1;
            if (dragging) {
                goome_draw_rect(c, all.x, all.y, all.w, t - all.y, 0x80000000);
                goome_draw_rect(c, all.x, t + h, all.w, all.y + all.h - t - h, 0x80000000);
                goome_draw_rect(c, all.x, t, l - all.x, h, 0x80000000);
                goome_draw_rect(c, l + w, t, all.x + all.w - l - w, h, 0x80000000);
                goome_draw_frame(c, l, t, w, h, 2, 0xFF6FDC5F);
                char size[32];
                snprintf(size, sizeof(size), "%.0f x %.0f", w, h);
                goome_draw_round_rect(c, l, t + h + 6, goome_text_width(c, size, 0) + 16, 24, 6, 0xE01B1D22);
                goome_draw_text(c, l + 8, t + h + 10, size, 0, 0xFFFFFFFF);
            } else {
                goome_draw_rect(c, all.x, all.y, all.w, all.h, 0x80000000);
                goome_text(c, font, 18, all.x + all.w / 2.0 - 120, all.y + 40, "drag a box, esc to stop", 0xFFFFFFFF);
            }
        }
        if (goome_now_ms() < note_until) {
            double w = goome_text_measure(font, 15, note, NULL) + 32;
            goome_draw_round_rect(c, all.x + all.w - w - 20, all.y + all.h - 64, w, 40, 10, 0xF01B1D22);
            goome_text(c, font, 15, all.x + all.w - w - 4, all.y + all.h - 54, note, 0xFFFFFFFF);
        }
        goome_end(g, c, &all, picking);
    }
    goome_font_free(font);
    goome_close(g);
    return 0;
}
