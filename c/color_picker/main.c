#include <stdio.h>
#include <string.h>

#include "goome.h"

#define GRID 11
#define CELL 10

int main(void)
{
    int err;
    goome *g = goome_open(&(goome_opts){.title = "color picker", .hide_self = 1}, &err);
    if (!g) {
        fprintf(stderr, "need a desktop to pick colors from: %s\n", goome_strerror(err));
        return 1;
    }
    goome_font *font = goome_font_system(NULL);
    if (!(goome_caps(g) & GOOME_CAP_CAPTURE)) fprintf(stderr, "this desktop wont let me see the screen, the loupe stays empty\n");
    goome_hotkey(g, "alt+p", 1);
    char hex[16] = "", copied[16] = "";
    int running = 1;
    goome_pacer pace = {0};
    while (running) {
        goome_event ev;
        while (goome_poll(g, &ev, goome_pace(&pace, 30)) > 0) {
            if (ev.type == GOOME_QUIT) running = 0;
            if (ev.type == GOOME_HOTKEY && hex[0] && goome_clipboard_set(g, hex) == GOOME_OK) {
                snprintf(copied, sizeof(copied), "%s", hex);
                printf("copied %s\n", hex);
                fflush(stdout);
            }
        }
        if (!running) break;
        double px, py;
        int buttons;
        goome_canvas *c = goome_begin(g, 0);
        if (!c) continue;
        goome_clear(c);
        if (goome_pointer(g, &px, &py, &buttons)) {
            goome_rect r = {(int)px - GRID / 2, (int)py - GRID / 2, GRID, GRID};
            goome_image img;
            double size = GRID * CELL, x = px + 24, y = py + 24;
            goome_draw_shadow(c, x, y, size + 16, size + 52, 12, 14, 0x70000000);
            goome_draw_round_rect(c, x, y, size + 16, size + 52, 12, 0xF01B1D22);
            if (goome_capture(g, 0, &r, &img) == GOOME_OK) {
                for (int j = 0; j < GRID && j < img.h; j++)
                    for (int i = 0; i < GRID && i < img.w; i++)
                        goome_draw_rect(c, x + 8 + i * CELL, y + 8 + j * CELL, CELL, CELL, img.px[(size_t)j * img.stride + i]);
                uint32_t mid = img.px[(size_t)(img.h / 2) * img.stride + img.w / 2];
                snprintf(hex, sizeof(hex), "#%06X", mid & 0xFFFFFF);
                goome_draw_frame(c, x + 8 + GRID / 2 * CELL, y + 8 + GRID / 2 * CELL, CELL, CELL, 2, 0xFFFFFFFF);
                goome_draw_round_rect(c, x + 8, y + size + 16, 28, 28, 6, mid);
                goome_text(c, font, 15, x + 44, y + size + 14, hex, 0xFFFFFFFF);
                goome_text(c, font, 11, x + 44, y + size + 32, copied[0] && !strcmp(copied, hex) ? "copied" : "alt+p to copy", 0xFF8A93A3);
            }
        }
        goome_end(g, c, NULL, 0);
    }
    goome_font_free(font);
    goome_close(g);
    return 0;
}
