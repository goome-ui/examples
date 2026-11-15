#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "goome.h"

static uint32_t crc_table[256];

static uint32_t crc(uint32_t c, const unsigned char *p, size_t n)
{
    if (!crc_table[1])
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t v = i;
            for (int k = 0; k < 8; k++) v = v & 1 ? 0xEDB88320u ^ (v >> 1) : v >> 1;
            crc_table[i] = v;
        }
    c = ~c;
    while (n--) c = crc_table[(c ^ *p++) & 255] ^ (c >> 8);
    return ~c;
}

static void put32(unsigned char *p, uint32_t v)
{
    p[0] = v >> 24, p[1] = v >> 16, p[2] = v >> 8, p[3] = v;
}

static void chunk(FILE *f, const char *type, const unsigned char *data, size_t n)
{
    unsigned char head[8];
    put32(head, (uint32_t)n);
    memcpy(head + 4, type, 4);
    uint32_t c = crc(crc(0, head + 4, 4), data, n);
    unsigned char tail[4];
    put32(tail, c);
    fwrite(head, 1, 8, f);
    fwrite(data, 1, n, f);
    fwrite(tail, 1, 4, f);
}

static int save_png(const char *path, const goome_image *im)
{
    size_t row = (size_t)im->w * 4 + 1, raw = row * im->h, blocks = (raw + 65534) / 65535;
    unsigned char *px = malloc(raw), *z = malloc(raw + blocks * 5 + 6);
    if (!px || !z) return free(px), free(z), 1;
    for (int y = 0; y < im->h; y++) {
        unsigned char *o = px + row * y;
        *o++ = 0;
        for (int x = 0; x < im->w; x++, o += 4) {
            uint32_t s = im->px[(size_t)y * im->stride + x], a = s >> 24;
            for (int k = 0; k < 3; k++) {
                uint32_t c = (s >> (16 - 8 * k)) & 255;
                o[k] = (unsigned char)(a ? (c * 255 + a / 2) / a : 0);
            }
            o[3] = (unsigned char)a;
        }
    }
    size_t n = 0;
    uint32_t s1 = 1, s2 = 0;
    z[n++] = 0x78, z[n++] = 0x01;
    for (size_t at = 0; at < raw; at += 65535) {
        size_t len = raw - at < 65535 ? raw - at : 65535;
        z[n++] = at + len == raw;
        z[n++] = len & 255, z[n++] = len >> 8, z[n++] = ~len & 255, z[n++] = (~len >> 8) & 255;
        memcpy(z + n, px + at, len);
        n += len;
    }
    for (size_t i = 0; i < raw; i++) s1 = (s1 + px[i]) % 65521, s2 = (s2 + s1) % 65521;
    put32(z + n, s2 << 16 | s1);
    n += 4;
    unsigned char ihdr[13] = {0};
    put32(ihdr, (uint32_t)im->w), put32(ihdr + 4, (uint32_t)im->h);
    ihdr[8] = 8, ihdr[9] = 6;
    FILE *f = fopen(path, "wb");
    if (f) {
        fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
        chunk(f, "IHDR", ihdr, 13);
        chunk(f, "IDAT", z, n);
        chunk(f, "IEND", NULL, 0);
        fclose(f);
    }
    free(px), free(z);
    return !f;
}

static void click(goome *g, double x, double y)
{
    goome_event e;
    memset(&e, 0, sizeof(e));
    e.button = 1, e.x = x, e.y = y;
    e.type = GOOME_DOWN, goome_push(g, &e);
    e.type = GOOME_UP, goome_push(g, &e);
}

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "offscreen.png";
    int err;
    goome *g = goome_open(&(goome_opts){.mode = GOOME_MODE_OFFSCREEN, .width = 360, .height = 300, .scale = 2}, &err);
    if (!g) {
        fprintf(stderr, "cant open offscreen: %s\n", goome_strerror(err));
        return 1;
    }
    goome_ui *u = goome_ui_new(g);
    goome_ui_theme(u)->anim_ms = 0;
    double x = 10, y = 10, w = 340, h = 0, health = 0.72;
    int music = 1, hard = 0, picked = 1;
    static const char *const modes[] = {"Easy", "Normal", "Hard"};
    for (int frame = 0; frame < 6; frame++) {
        if (frame == 2) click(g, 30, 171);
        goome_event ev;
        while (goome_poll(g, &ev, 0) > 0) goome_ui_feed(u, &ev);
        if (!goome_ui_begin(u, 0)) continue;
        if (goome_ui_window(u, "Pause menu", &x, &y, &w, &h, 0, NULL)) {
            goome_ui_label(u, "no window here, just pixels in memory");
            goome_ui_progress(u, health, "health");
            goome_ui_toggle(u, "Music", &music);
            goome_ui_checkbox(u, "Hard mode", &hard);
            goome_ui_combo(u, "Difficulty", &picked, modes, 3);
            goome_ui_row(u, 2);
            goome_ui_button(u, "Resume");
            goome_ui_button(u, "Quit");
        }
        goome_ui_window_end(u);
        goome_ui_end(u);
    }
    goome_image im;
    int bad = goome_frame(g, &im) != GOOME_OK || save_png(path, &im);
    printf("%s %dx%d, hard mode %s\n", bad ? "couldnt write" : path, im.w, im.h, hard ? "on" : "off");
    goome_ui_free(u);
    goome_close(g);
    return bad;
}
