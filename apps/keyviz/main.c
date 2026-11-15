#define _DEFAULT_SOURCE
#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <direct.h>
#ifdef _MSC_VER
#pragma comment(lib, "user32.lib")
#endif
#else
#include <dlfcn.h>
#include <sys/stat.h>
#endif
#if defined(__linux__)
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "goome.h"

#define MAX_MONS 8
#define MAX_CHIPS 6
#define MAX_RIPPLES 16
#define MAX_PARTS 6

enum { K_SHIFT = 0xA0, K_CTRL, K_ALT, K_SUPER, K_CAPS, K_MENU, K_PRINT };
enum { HOTKEY_SETTINGS = 1, HOTKEY_PAUSE };
enum { BOTTOM_CENTER, BOTTOM_LEFT, BOTTOM_RIGHT, TOP_CENTER, TOP_LEFT, TOP_RIGHT, SPOTS };

static const char *const spot_names[SPOTS] = {"Bottom middle", "Bottom left", "Bottom right", "Top middle", "Top left", "Top right"};

typedef struct {
    int code, down;
} keyev;

typedef struct {
    int text;
    char s[160];
    int nparts;
    int parts[MAX_PARTS];
    int count;
    long born, touched;
} chip;

typedef struct {
    double x, y;
    int button;
    long born;
} ripple;

typedef struct {
    int spot, mon, typing, clicks, mouse, paused;
    double size, fade;
} prefs;

static const prefs default_prefs = {BOTTOM_CENTER, 0, 1, 1, 1, 0, 1.0, 2.5};

static chip chips[MAX_CHIPS];
static int nchips;
static ripple ripples[MAX_RIPPLES];
static char status[200];

#if defined(_WIN32)

static const struct {
    int vk, code;
} vkmap[] = {
    {VK_BACK, GOOME_K_BACKSPACE}, {VK_TAB, GOOME_K_TAB}, {VK_RETURN, GOOME_K_ENTER}, {VK_ESCAPE, GOOME_K_ESC}, {VK_SPACE, ' '},
    {VK_DELETE, GOOME_K_DELETE}, {VK_LEFT, GOOME_K_LEFT}, {VK_RIGHT, GOOME_K_RIGHT}, {VK_UP, GOOME_K_UP}, {VK_DOWN, GOOME_K_DOWN},
    {VK_HOME, GOOME_K_HOME}, {VK_END, GOOME_K_END}, {VK_PRIOR, GOOME_K_PGUP}, {VK_NEXT, GOOME_K_PGDN}, {VK_INSERT, GOOME_K_INSERT},
    {VK_SHIFT, K_SHIFT}, {VK_CONTROL, K_CTRL}, {VK_MENU, K_ALT}, {VK_LWIN, K_SUPER}, {VK_RWIN, K_SUPER}, {VK_CAPITAL, K_CAPS},
    {VK_APPS, K_MENU}, {VK_SNAPSHOT, K_PRINT}, {VK_OEM_1, ';'}, {VK_OEM_PLUS, '='}, {VK_OEM_COMMA, ','}, {VK_OEM_MINUS, '-'},
    {VK_OEM_PERIOD, '.'}, {VK_OEM_2, '/'}, {VK_OEM_3, '`'}, {VK_OEM_4, '['}, {VK_OEM_5, '\\'}, {VK_OEM_6, ']'}, {VK_OEM_7, '\''},
};
static unsigned char was_down[256];

static int src_open(void) { return 1; }

static int src_poll(keyev *out, int max)
{
    int n = 0;
    for (int vk = 0; vk < 256 && n < max; vk++) {
        int code = 0;
        if (vk >= 'A' && vk <= 'Z') code = vk - 'A' + 'a';
        else if (vk >= '0' && vk <= '9') code = vk;
        else if (vk >= VK_F1 && vk <= VK_F12) code = GOOME_K_F1 + vk - VK_F1;
        else
            for (int i = 0; i < (int)(sizeof(vkmap) / sizeof(*vkmap)); i++)
                if (vkmap[i].vk == vk) code = vkmap[i].code;
        if (!code) continue;
        int down = (GetAsyncKeyState(vk) & 0x8000) != 0;
        if (vk == VK_LWIN || vk == VK_RWIN) {
            if (vk == VK_RWIN) continue;
            down = down || (GetAsyncKeyState(VK_RWIN) & 0x8000) != 0;
        }
        if (down != was_down[vk]) out[n].code = code, out[n].down = down, n++, was_down[vk] = (unsigned char)down;
    }
    return n;
}

static int src_buttons(int *b) { return 0; }
static int src_scroll(void) { return 0; }

#elif defined(__APPLE__)

typedef unsigned char (*key_state_fn)(int, unsigned short);
typedef unsigned char (*access_fn)(void);
static key_state_fn key_state;
static const struct {
    int kc, code;
} macmap[] = {
    {0x00, 'a'}, {0x01, 's'}, {0x02, 'd'}, {0x03, 'f'}, {0x04, 'h'}, {0x05, 'g'}, {0x06, 'z'}, {0x07, 'x'}, {0x08, 'c'}, {0x09, 'v'},
    {0x0B, 'b'}, {0x0C, 'q'}, {0x0D, 'w'}, {0x0E, 'e'}, {0x0F, 'r'}, {0x10, 'y'}, {0x11, 't'}, {0x12, '1'}, {0x13, '2'}, {0x14, '3'},
    {0x15, '4'}, {0x16, '6'}, {0x17, '5'}, {0x18, '='}, {0x19, '9'}, {0x1A, '7'}, {0x1B, '-'}, {0x1C, '8'}, {0x1D, '0'}, {0x1E, ']'},
    {0x1F, 'o'}, {0x20, 'u'}, {0x21, '['}, {0x22, 'i'}, {0x23, 'p'}, {0x24, GOOME_K_ENTER}, {0x25, 'l'}, {0x26, 'j'}, {0x27, '\''},
    {0x28, 'k'}, {0x29, ';'}, {0x2A, '\\'}, {0x2B, ','}, {0x2C, '/'}, {0x2D, 'n'}, {0x2E, 'm'}, {0x2F, '.'}, {0x30, GOOME_K_TAB},
    {0x31, ' '}, {0x32, '`'}, {0x33, GOOME_K_BACKSPACE}, {0x35, GOOME_K_ESC}, {0x36, K_SUPER}, {0x37, K_SUPER}, {0x38, K_SHIFT},
    {0x39, K_CAPS}, {0x3A, K_ALT}, {0x3B, K_CTRL}, {0x3C, K_SHIFT}, {0x3D, K_ALT}, {0x3E, K_CTRL}, {0x60, GOOME_K_F1 + 4},
    {0x61, GOOME_K_F1 + 5}, {0x62, GOOME_K_F1 + 6}, {0x63, GOOME_K_F1 + 2}, {0x64, GOOME_K_F1 + 7}, {0x65, GOOME_K_F1 + 8},
    {0x67, GOOME_K_F1 + 10}, {0x6D, GOOME_K_F1 + 9}, {0x6F, GOOME_K_F1 + 11}, {0x73, GOOME_K_HOME}, {0x74, GOOME_K_PGUP},
    {0x75, GOOME_K_DELETE}, {0x76, GOOME_K_F1 + 3}, {0x77, GOOME_K_END}, {0x78, GOOME_K_F1 + 1}, {0x79, GOOME_K_PGDN}, {0x7A, GOOME_K_F1},
    {0x7B, GOOME_K_LEFT}, {0x7C, GOOME_K_RIGHT}, {0x7D, GOOME_K_DOWN}, {0x7E, GOOME_K_UP},
};
static unsigned char was_down[128];

static int src_open(void)
{
    void *cg = dlopen("/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics", RTLD_LAZY);
    if (!cg) cg = dlopen("/System/Library/Frameworks/ApplicationServices.framework/ApplicationServices", RTLD_LAZY);
    if (!cg) {
        snprintf(status, sizeof(status), "cant load CoreGraphics, no keys sorry");
        return 0;
    }
    key_state = (key_state_fn)dlsym(cg, "CGEventSourceKeyState");
    access_fn preflight = (access_fn)dlsym(cg, "CGPreflightListenEventAccess");
    access_fn request = (access_fn)dlsym(cg, "CGRequestListenEventAccess");
    if (preflight && !preflight()) {
        if (request) request();
        snprintf(status, sizeof(status), "allow keyviz (or your terminal) in System Settings > Privacy > Input Monitoring, then start it again");
    }
    return key_state != NULL;
}

static int src_poll(keyev *out, int max)
{
    int n = 0;
    if (!key_state) return 0;
    for (int i = 0; i < (int)(sizeof(macmap) / sizeof(*macmap)) && n < max; i++) {
        int kc = macmap[i].kc, down = key_state(0, (unsigned short)kc) != 0;
        if (down != was_down[kc]) out[n].code = macmap[i].code, out[n].down = down, n++, was_down[kc] = (unsigned char)down;
    }
    return n;
}

static int src_buttons(int *b) { return 0; }
static int src_scroll(void) { return 0; }

#elif defined(__linux__)

static const struct {
    int key, code;
} evmap[] = {
    {KEY_ESC, GOOME_K_ESC}, {KEY_1, '1'}, {KEY_2, '2'}, {KEY_3, '3'}, {KEY_4, '4'}, {KEY_5, '5'}, {KEY_6, '6'}, {KEY_7, '7'}, {KEY_8, '8'},
    {KEY_9, '9'}, {KEY_0, '0'}, {KEY_MINUS, '-'}, {KEY_EQUAL, '='}, {KEY_BACKSPACE, GOOME_K_BACKSPACE}, {KEY_TAB, GOOME_K_TAB},
    {KEY_Q, 'q'}, {KEY_W, 'w'}, {KEY_E, 'e'}, {KEY_R, 'r'}, {KEY_T, 't'}, {KEY_Y, 'y'}, {KEY_U, 'u'}, {KEY_I, 'i'}, {KEY_O, 'o'},
    {KEY_P, 'p'}, {KEY_LEFTBRACE, '['}, {KEY_RIGHTBRACE, ']'}, {KEY_ENTER, GOOME_K_ENTER}, {KEY_KPENTER, GOOME_K_ENTER},
    {KEY_LEFTCTRL, K_CTRL}, {KEY_RIGHTCTRL, K_CTRL}, {KEY_A, 'a'}, {KEY_S, 's'}, {KEY_D, 'd'}, {KEY_F, 'f'}, {KEY_G, 'g'}, {KEY_H, 'h'},
    {KEY_J, 'j'}, {KEY_K, 'k'}, {KEY_L, 'l'}, {KEY_SEMICOLON, ';'}, {KEY_APOSTROPHE, '\''}, {KEY_GRAVE, '`'}, {KEY_LEFTSHIFT, K_SHIFT},
    {KEY_RIGHTSHIFT, K_SHIFT}, {KEY_BACKSLASH, '\\'}, {KEY_Z, 'z'}, {KEY_X, 'x'}, {KEY_C, 'c'}, {KEY_V, 'v'}, {KEY_B, 'b'}, {KEY_N, 'n'},
    {KEY_M, 'm'}, {KEY_COMMA, ','}, {KEY_DOT, '.'}, {KEY_SLASH, '/'}, {KEY_LEFTALT, K_ALT}, {KEY_RIGHTALT, K_ALT}, {KEY_SPACE, ' '},
    {KEY_CAPSLOCK, K_CAPS}, {KEY_F1, GOOME_K_F1}, {KEY_F2, GOOME_K_F1 + 1}, {KEY_F3, GOOME_K_F1 + 2}, {KEY_F4, GOOME_K_F1 + 3},
    {KEY_F5, GOOME_K_F1 + 4}, {KEY_F6, GOOME_K_F1 + 5}, {KEY_F7, GOOME_K_F1 + 6}, {KEY_F8, GOOME_K_F1 + 7}, {KEY_F9, GOOME_K_F1 + 8},
    {KEY_F10, GOOME_K_F1 + 9}, {KEY_F11, GOOME_K_F1 + 10}, {KEY_F12, GOOME_K_F1 + 11}, {KEY_HOME, GOOME_K_HOME}, {KEY_UP, GOOME_K_UP},
    {KEY_PAGEUP, GOOME_K_PGUP}, {KEY_LEFT, GOOME_K_LEFT}, {KEY_RIGHT, GOOME_K_RIGHT}, {KEY_END, GOOME_K_END}, {KEY_DOWN, GOOME_K_DOWN},
    {KEY_PAGEDOWN, GOOME_K_PGDN}, {KEY_INSERT, GOOME_K_INSERT}, {KEY_DELETE, GOOME_K_DELETE}, {KEY_LEFTMETA, K_SUPER},
    {KEY_RIGHTMETA, K_SUPER}, {KEY_COMPOSE, K_MENU}, {KEY_SYSRQ, K_PRINT},
};

enum {
    XK_BACKSPACE = 0xff08, XK_TAB = 0xff09, XK_RETURN = 0xff0d, XK_ESCAPE = 0xff1b, XK_DELETE = 0xffff, XK_HOME = 0xff50,
    XK_LEFT = 0xff51, XK_UP = 0xff52, XK_RIGHT = 0xff53, XK_DOWN = 0xff54, XK_PRIOR = 0xff55, XK_NEXT = 0xff56, XK_END = 0xff57,
    XK_PRINT = 0xff61, XK_INSERT = 0xff63, XK_MENU = 0xff67, XK_KP_ENTER = 0xff8d, XK_F1 = 0xffbe, XK_F12 = 0xffc9,
    XK_SHIFT_L = 0xffe1, XK_SHIFT_R = 0xffe2, XK_CONTROL_L = 0xffe3, XK_CONTROL_R = 0xffe4, XK_CAPS_LOCK = 0xffe5,
    XK_META_L = 0xffe7, XK_META_R = 0xffe8, XK_ALT_L = 0xffe9, XK_ALT_R = 0xffea, XK_SUPER_L = 0xffeb, XK_SUPER_R = 0xffec,
    XK_ISO_LEVEL3_SHIFT = 0xfe03,
};

#define MAX_DEVS 32
static int kbd_fd[MAX_DEVS], nkbd, mouse_fd[MAX_DEVS], nmouse, ev_buttons, ev_scroll;
static int use_x11;
static void *xdpy;
static int (*x_query_keymap)(void *, char *);
static unsigned long (*x_kc_to_sym)(void *, unsigned char, int, int);
static char x_was[32];
static unsigned char ev_was[KEY_MAX + 1];

static int has_bit(const unsigned long *bits, int n) { return (bits[n / (8 * sizeof(long))] >> (n % (8 * sizeof(long)))) & 1; }

static int src_open(void)
{
    for (int i = 0; i < 64; i++) {
        char path[32];
        snprintf(path, sizeof(path), "/dev/input/event%d", i);
        int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) continue;
        unsigned long keys[KEY_MAX / (8 * sizeof(long)) + 1] = {0}, rels[REL_MAX / (8 * sizeof(long)) + 1] = {0};
        ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keys)), keys);
        ioctl(fd, EVIOCGBIT(EV_REL, sizeof(rels)), rels);
        if (has_bit(keys, KEY_A) && has_bit(keys, KEY_SPACE) && nkbd < MAX_DEVS) kbd_fd[nkbd++] = fd;
        else if (has_bit(keys, BTN_LEFT) && has_bit(rels, REL_X) && nmouse < MAX_DEVS) mouse_fd[nmouse++] = fd;
        else close(fd);
    }
    if (nkbd) return 1;
    const char *wl = getenv("WAYLAND_DISPLAY");
    void *x11 = getenv("DISPLAY") ? dlopen("libX11.so.6", RTLD_LAZY) : NULL;
    if (x11) {
        void *(*x_open)(const char *) = (void *(*)(const char *))dlsym(x11, "XOpenDisplay");
        x_query_keymap = (int (*)(void *, char *))dlsym(x11, "XQueryKeymap");
        x_kc_to_sym = (unsigned long (*)(void *, unsigned char, int, int))dlsym(x11, "XkbKeycodeToKeysym");
        xdpy = x_open && x_query_keymap && x_kc_to_sym ? x_open(NULL) : NULL;
    }
    if (xdpy) use_x11 = 1;
    if (wl && *wl)
        snprintf(status, sizeof(status), "on wayland keyviz reads keys from /dev/input. run: sudo usermod -aG input $USER, then log out and back in");
    else if (!xdpy)
        snprintf(status, sizeof(status), "cant read keys here. add yourself to the input group: sudo usermod -aG input $USER");
    return use_x11;
}

static int x11_code(unsigned long sym)
{
    if (sym >= 'a' && sym <= 'z') return (int)sym;
    if (sym >= 'A' && sym <= 'Z') return (int)sym - 'A' + 'a';
    if (sym >= 0x20 && sym < 0x7f) return (int)sym;
    if (sym >= XK_F1 && sym <= XK_F12) return GOOME_K_F1 + (int)(sym - XK_F1);
    switch (sym) {
    case XK_BACKSPACE: return GOOME_K_BACKSPACE;
    case XK_TAB: return GOOME_K_TAB;
    case XK_RETURN:
    case XK_KP_ENTER: return GOOME_K_ENTER;
    case XK_ESCAPE: return GOOME_K_ESC;
    case XK_DELETE: return GOOME_K_DELETE;
    case XK_HOME: return GOOME_K_HOME;
    case XK_END: return GOOME_K_END;
    case XK_LEFT: return GOOME_K_LEFT;
    case XK_RIGHT: return GOOME_K_RIGHT;
    case XK_UP: return GOOME_K_UP;
    case XK_DOWN: return GOOME_K_DOWN;
    case XK_PRIOR: return GOOME_K_PGUP;
    case XK_NEXT: return GOOME_K_PGDN;
    case XK_INSERT: return GOOME_K_INSERT;
    case XK_PRINT: return K_PRINT;
    case XK_MENU: return K_MENU;
    case XK_SHIFT_L:
    case XK_SHIFT_R: return K_SHIFT;
    case XK_CONTROL_L:
    case XK_CONTROL_R: return K_CTRL;
    case XK_ALT_L:
    case XK_ALT_R:
    case XK_META_L:
    case XK_META_R:
    case XK_ISO_LEVEL3_SHIFT: return K_ALT;
    case XK_SUPER_L:
    case XK_SUPER_R: return K_SUPER;
    case XK_CAPS_LOCK: return K_CAPS;
    }
    return 0;
}

static int src_poll(keyev *out, int max)
{
    int n = 0;
    if (nkbd) {
        struct input_event e;
        for (int d = 0; d < nkbd; d++)
            while (n < max && read(kbd_fd[d], &e, sizeof(e)) == (ssize_t)sizeof(e)) {
                if (e.type != EV_KEY || e.value == 2 || e.code > KEY_MAX) continue;
                for (int i = 0; i < (int)(sizeof(evmap) / sizeof(*evmap)); i++)
                    if (evmap[i].key == e.code && ev_was[e.code] != e.value) {
                        out[n].code = evmap[i].code, out[n].down = e.value, n++;
                        ev_was[e.code] = (unsigned char)e.value;
                        break;
                    }
            }
    } else if (use_x11) {
        char now[32];
        x_query_keymap(xdpy, now);
        for (int kc = 8; kc < 256 && n < max; kc++) {
            int was = (x_was[kc / 8] >> (kc % 8)) & 1, is = (now[kc / 8] >> (kc % 8)) & 1;
            if (was == is) continue;
            int code = x11_code(x_kc_to_sym(xdpy, (unsigned char)kc, 0, 0));
            if (code) out[n].code = code, out[n].down = is, n++;
        }
        memcpy(x_was, now, sizeof(now));
    }
    return n;
}

static int src_buttons(int *b)
{
    if (!nmouse) return 0;
    struct input_event e;
    for (int d = 0; d < nmouse; d++)
        while (read(mouse_fd[d], &e, sizeof(e)) == (ssize_t)sizeof(e)) {
            int bit = e.code == BTN_LEFT ? 1 : e.code == BTN_MIDDLE ? 2 : e.code == BTN_RIGHT ? 4 : 0;
            if (e.type == EV_KEY && bit) ev_buttons = e.value ? ev_buttons | bit : ev_buttons & ~bit;
            if (e.type == EV_REL && e.code == REL_WHEEL) ev_scroll += e.value > 0 ? -1 : 1;
        }
    *b = ev_buttons;
    return 1;
}

static int src_scroll(void)
{
    int s = ev_scroll;
    ev_scroll = 0;
    return s;
}

#else

static int src_open(void)
{
    snprintf(status, sizeof(status), "this os cant hand out keys to keyviz yet");
    return 0;
}
static int src_poll(keyev *out, int max) { return 0; }
static int src_buttons(int *b) { return 0; }
static int src_scroll(void) { return 0; }

#endif

static void config_dir(char *out, int cap)
{
#ifdef _WIN32
    const char *base = getenv("APPDATA");
    snprintf(out, cap, "%s\\goome-keyviz", base ? base : ".");
    _mkdir(out);
#elif defined(__APPLE__)
    const char *home = getenv("HOME");
    snprintf(out, cap, "%s/Library/Application Support/goome-keyviz", home ? home : ".");
    mkdir(out, 0700);
#else
    const char *xdg = getenv("XDG_CONFIG_HOME"), *home = getenv("HOME");
    if (xdg && *xdg) snprintf(out, cap, "%s/goome-keyviz", xdg);
    else snprintf(out, cap, "%s/.config/goome-keyviz", home ? home : ".");
    mkdir(out, 0700);
#endif
}

static void load_prefs(const char *path, prefs *p)
{
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[96];
    while (fgets(line, sizeof(line), f)) {
        sscanf(line, "spot %d", &p->spot);
        sscanf(line, "monitor %d", &p->mon);
        sscanf(line, "typing %d", &p->typing);
        sscanf(line, "clicks %d", &p->clicks);
        sscanf(line, "mouse %d", &p->mouse);
        sscanf(line, "size %lf", &p->size);
        sscanf(line, "fade %lf", &p->fade);
    }
    fclose(f);
    if (p->spot < 0 || p->spot >= SPOTS) p->spot = BOTTOM_CENTER;
    if (p->size < 0.5 || p->size > 3) p->size = 1;
    if (p->fade < 0.5 || p->fade > 10) p->fade = 2.5;
}

static void save_prefs(const char *path, const prefs *p)
{
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "spot %d\nmonitor %d\ntyping %d\nclicks %d\nmouse %d\nsize %.2f\nfade %.2f\n", p->spot, p->mon, p->typing, p->clicks, p->mouse, p->size, p->fade);
    fclose(f);
}

static const char *key_name(int code)
{
    static char one[2];
#ifdef __APPLE__
    const char *super = "Cmd", *alt = "Opt";
#elif defined(_WIN32)
    const char *super = "Win", *alt = "Alt";
#else
    const char *super = "Super", *alt = "Alt";
#endif
    static char fkey[8];
    if (code >= GOOME_K_F1 && code <= GOOME_K_F12) {
        snprintf(fkey, sizeof(fkey), "F%d", code - GOOME_K_F1 + 1);
        return fkey;
    }
    switch (code) {
    case K_SHIFT: return "Shift";
    case K_CTRL: return "Ctrl";
    case K_ALT: return alt;
    case K_SUPER: return super;
    case K_CAPS: return "Caps";
    case K_MENU: return "Menu";
    case K_PRINT: return "PrtSc";
    case GOOME_K_BACKSPACE: return "Backspace";
    case GOOME_K_TAB: return "Tab";
    case GOOME_K_ENTER: return "Enter";
    case GOOME_K_ESC: return "Esc";
    case ' ': return "Space";
    case GOOME_K_DELETE: return "Del";
    case GOOME_K_HOME: return "Home";
    case GOOME_K_END: return "End";
    case GOOME_K_PGUP: return "PgUp";
    case GOOME_K_PGDN: return "PgDn";
    case GOOME_K_INSERT: return "Ins";
    }
    one[0] = (char)(code >= 'a' && code <= 'z' ? code - 'a' + 'A' : code), one[1] = 0;
    return one;
}

static int shifted(int code)
{
    static const char from[] = "1234567890-=[]\\;',./`", to[] = "!@#$%^&*()_+{}|:\"<>?~";
    if (code >= 'a' && code <= 'z') return code - 'a' + 'A';
    const char *p = strchr(from, code);
    return p ? to[p - from] : code;
}

static chip *new_chip(long now)
{
    if (nchips == MAX_CHIPS) memmove(chips, chips + 1, sizeof(chip) * (MAX_CHIPS - 1)), nchips--;
    chip *c = &chips[nchips++];
    memset(c, 0, sizeof(*c));
    c->born = c->touched = now, c->count = 1;
    return c;
}

typedef struct {
    int held[4], alone;
} mods;

static int mod_index(int code) { return code >= K_SHIFT && code <= K_SUPER ? code - K_SHIFT : -1; }

static void combo_chip(const prefs *p, const int *parts, int n, long now)
{
    chip *last = nchips ? &chips[nchips - 1] : NULL;
    if (last && !last->text && last->nparts == n && !memcmp(last->parts, parts, sizeof(int) * (size_t)n) && now - last->touched < p->fade * 1000) {
        last->count++, last->touched = now;
        return;
    }
    chip *c = new_chip(now);
    c->nparts = n;
    memcpy(c->parts, parts, sizeof(int) * (size_t)n);
}

static void key_down(mods *m, const prefs *p, int code, long now)
{
    int mi = mod_index(code);
    if (mi >= 0) {
        if (!m->held[mi]) m->alone = code;
        m->held[mi] = 1;
        return;
    }
    m->alone = 0;
    if (code == K_CAPS) return;
    int combo = m->held[1] || m->held[2] || m->held[3];
    int printable = code >= ' ' && code < 127;
    chip *last = nchips ? &chips[nchips - 1] : NULL;
    if (printable && !combo) {
        if (!p->typing) return;
        int ch = m->held[0] ? shifted(code) : code;
        if (!last || !last->text || now - last->touched > 1600 || strlen(last->s) > 150) last = new_chip(now), last->text = 1;
        size_t l = strlen(last->s);
        last->s[l] = (char)ch, last->s[l + 1] = 0;
        last->touched = now;
        return;
    }
    int parts[MAX_PARTS], n = 0;
    if (m->held[1]) parts[n++] = K_CTRL;
    if (m->held[2]) parts[n++] = K_ALT;
    if (m->held[0]) parts[n++] = K_SHIFT;
    if (m->held[3]) parts[n++] = K_SUPER;
    parts[n++] = code;
    combo_chip(p, parts, n, now);
}

static void key_up(mods *m, const prefs *p, int code, long now)
{
    int mi = mod_index(code);
    if (mi < 0) return;
    m->held[mi] = 0;
    if (m->alone == code && code != K_SHIFT) combo_chip(p, &code, 1, now);
    m->alone = 0;
}

static uint32_t alpha(uint32_t c, double a)
{
    if (a <= 0) return 0;
    if (a > 1) a = 1;
    return (uint32_t)(((c >> 24) & 0xFF) * a + 0.5) << 24 | (c & 0xFFFFFF);
}

static double ease_out(double t) { return t >= 1 ? 1 : 1 - (1 - t) * (1 - t) * (1 - t); }

static void arrow_glyph(goome_canvas *c, double cx, double cy, double s, int code, uint32_t col)
{
    if (code == GOOME_K_LEFT) goome_draw_triangle(c, cx - s, cy, cx + s * 0.7, cy - s, cx + s * 0.7, cy + s, col);
    if (code == GOOME_K_RIGHT) goome_draw_triangle(c, cx + s, cy, cx - s * 0.7, cy - s, cx - s * 0.7, cy + s, col);
    if (code == GOOME_K_UP) goome_draw_triangle(c, cx, cy - s, cx - s, cy + s * 0.7, cx + s, cy + s * 0.7, col);
    if (code == GOOME_K_DOWN) goome_draw_triangle(c, cx, cy + s, cx - s, cy - s * 0.7, cx + s, cy - s * 0.7, col);
}

static int is_arrow(int code) { return code >= GOOME_K_LEFT && code <= GOOME_K_DOWN; }

static double cap_width(goome_font *f, double k, int code)
{
    if (is_arrow(code)) return 40 * k;
    double w = goome_text_measure(f, 17 * k, key_name(code), NULL) + 24 * k;
    return w < 40 * k ? 40 * k : w;
}

static double chip_width(goome_font *f, double k, const chip *ch)
{
    if (ch->text) {
        const char *s = ch->s;
        while (goome_text_measure(f, 20 * k, s, NULL) > 560 * k && *s) s++;
        return goome_text_measure(f, 20 * k, s, NULL) + 32 * k;
    }
    double w = 16 * k;
    for (int i = 0; i < ch->nparts; i++) w += cap_width(f, k, ch->parts[i]) + (i ? 22 * k : 0);
    if (ch->count > 1) {
        char cnt[16];
        snprintf(cnt, sizeof(cnt), "x%d", ch->count);
        w += goome_text_measure(f, 17 * k, cnt, NULL) + 12 * k;
    }
    return w;
}

static void draw_chip(goome_canvas *c, goome_font *f, double k, const chip *ch, double x, double y, double a)
{
    double h = 50 * k, w = chip_width(f, k, ch);
    goome_draw_shadow(c, x, y + 4 * k, w, h, 14 * k, 18 * k, alpha(0x70000000, a));
    goome_draw_rrect(c, x, y, w, h, goome_radius(14 * k), alpha(0xE6121418, a));
    goome_draw_rrect_line(c, x + 0.5, y + 0.5, w - 1, h - 1, goome_radius(14 * k), 1, alpha(0x1FFFFFFF, a));
    if (ch->text) {
        const char *s = ch->s;
        while (goome_text_measure(f, 20 * k, s, NULL) > 560 * k && *s) s++;
        double th = goome_text_line_height(f, 20 * k);
        goome_text(c, f, 20 * k, x + 16 * k, y + (h - th) / 2, s, alpha(0xFFF2F4F7, a));
        return;
    }
    double cx = x + 8 * k, ch_h = h - 14 * k, cy = y + 5 * k;
    for (int i = 0; i < ch->nparts; i++) {
        int code = ch->parts[i];
        if (i) {
            double pw = goome_text_measure(f, 16 * k, "+", NULL);
            goome_text(c, f, 16 * k, cx + (22 * k - pw) / 2, y + (h - goome_text_line_height(f, 16 * k)) / 2, "+", alpha(0xFF7D8696, a));
            cx += 22 * k;
        }
        double cw = cap_width(f, k, code);
        int mod = mod_index(code) >= 0 && i < ch->nparts - 1;
        uint32_t base = mod ? 0xFF2C3A2A : 0xFF2A2F38, face = mod ? 0xFF3C5238 : 0xFF3B414C, ink = mod ? 0xFFB9F2AE : 0xFFF2F4F7;
        goome_draw_rrect(c, cx, cy + 2 * k, cw, ch_h, goome_radius(8 * k), alpha(base, a));
        goome_draw_rrect_grad(c, cx, cy, cw, ch_h - 2 * k, goome_radius(8 * k), alpha(face, a), alpha(base, a), 0);
        if (is_arrow(code)) arrow_glyph(c, cx + cw / 2, cy + (ch_h - 2 * k) / 2, 6 * k, code, alpha(ink, a));
        else {
            const char *nm = key_name(code);
            double tw = goome_text_measure(f, 17 * k, nm, NULL), th = goome_text_line_height(f, 17 * k);
            goome_text(c, f, 17 * k, cx + (cw - tw) / 2, cy + (ch_h - 2 * k - th) / 2, nm, alpha(ink, a));
        }
        cx += cw;
    }
    if (ch->count > 1) {
        char cnt[16];
        snprintf(cnt, sizeof(cnt), "x%d", ch->count);
        goome_text(c, f, 17 * k, cx + 10 * k, y + (h - goome_text_line_height(f, 17 * k)) / 2, cnt, alpha(0xFF6FDC5F, a));
    }
}

static void draw_mouse(goome_canvas *c, double k, double x, double y, int buttons, int scroll, double a)
{
    double w = 34 * k, h = 50 * k, r = 15 * k;
    goome_draw_shadow(c, x, y + 4 * k, w, h, r, 16 * k, alpha(0x70000000, a));
    goome_draw_rrect(c, x, y, w, h, goome_radius(r), alpha(0xE6121418, a));
    goome_draw_rrect(c, x + 3 * k, y + 3 * k, w - 6 * k, h - 6 * k, goome_radius(r - 3 * k), alpha(0xFF2A2F38, a));
    double bh = 20 * k, mid = x + w / 2;
    if (buttons & 1) goome_draw_rrect(c, x + 3 * k, y + 3 * k, w / 2 - 4 * k, bh, (goome_radii){r - 3 * k, 0, 0, 0}, alpha(0xFF6FDC5F, a));
    if (buttons & 4) goome_draw_rrect(c, mid + 1 * k, y + 3 * k, w / 2 - 4 * k, bh, (goome_radii){0, r - 3 * k, 0, 0}, alpha(0xFF4FC3FF, a));
    goome_draw_rect(c, mid - 0.5 * k, y + 3 * k, 1 * k, bh, alpha(0xFF121418, a));
    goome_draw_rect(c, x + 3 * k, y + 3 * k + bh, w - 6 * k, 1 * k, alpha(0xFF121418, a));
    goome_draw_rrect(c, mid - 3 * k, y + 8 * k, 6 * k, 10 * k, goome_radius(3 * k), alpha(buttons & 2 ? 0xFFF0C24F : 0xFF5A6170, a));
    if (scroll) arrow_glyph(c, mid, y + h - 14 * k, 4 * k, scroll > 0 ? GOOME_K_DOWN : GOOME_K_UP, alpha(0xFFF0C24F, a));
}

int main(int argc, char **argv)
{
    char dir[1024], cfg[1100];
    config_dir(dir, sizeof(dir));
    if (argc >= 3 && !strcmp(argv[1], "--send")) {
        int rc = goome_send(dir, argv[2]);
        if (rc != GOOME_OK) fprintf(stderr, "keyviz: %s, is it running?\n", goome_strerror(rc));
        return rc != GOOME_OK;
    }
    int err;
    goome *g = goome_open(&(goome_opts){.title = "keyviz", .no_capture = 1, .state_dir = dir}, &err);
    if (!g) {
        fprintf(stderr, "keyviz: cant open an overlay here, %s\n", goome_strerror(err));
        return 1;
    }
    goome_ui *u = goome_ui_new(g);
    goome_ui_theme_make(goome_ui_theme(u), 0xFF6FDC5F, 1, 10);
    goome_font *font = goome_ui_theme(u)->font;
    int test = !strcmp(goome_backend_name(g), "test");
    snprintf(cfg, sizeof(cfg), test ? "keyviz.txt" : "%s/keyviz.txt", dir);
    prefs p = default_prefs;
    load_prefs(cfg, &p);
    if (test) {
        goome_ui_auto_grab(u, GOOME_WANT_MOUSE);
        goome_keyboard(g, 1);
    } else src_open();
    goome_hotkey(g, "alt+shift+k", HOTKEY_SETTINGS);
    goome_hotkey(g, "alt+shift+p", HOTKEY_PAUSE);

    goome_monitor mons[MAX_MONS];
    int nmons = goome_monitors(g, mons, MAX_MONS);
    if (p.mon >= nmons) p.mon = 0;
    mods m = {{0}, 0};
    int settings = 0, running = 1, buttons = 0, scroll = 0, prefs_dirty = 0, drawn = 1;
    long mouse_at = -100000, status_until = status[0] ? goome_now_ms() + 12000 : 0;
    double wx = 0, wy = 0, ww = 330, wh = 0;
    if (nmons > 0) wx = mons[p.mon].area.x + 60, wy = mons[p.mon].area.y + 60;
    goome_pacer pace = {0};
    while (running) {
        goome_event ev;
        long now = goome_now_ms();
        while (goome_poll(g, &ev, goome_pace(&pace, 60)) > 0) {
            if (ev.type == GOOME_QUIT) running = 0;
            if (ev.type == GOOME_MONITORS) {
                nmons = goome_monitors(g, mons, MAX_MONS);
                if (p.mon >= nmons) p.mon = 0;
            }
            if ((ev.type == GOOME_HOTKEY && ev.id == HOTKEY_SETTINGS) || (ev.type == GOOME_CONTROL && !strcmp(ev.msg, "settings"))) settings = !settings;
            if ((ev.type == GOOME_HOTKEY && ev.id == HOTKEY_PAUSE) || (ev.type == GOOME_CONTROL && !strcmp(ev.msg, "pause"))) p.paused = !p.paused;
            if (ev.type == GOOME_CONTROL && !strcmp(ev.msg, "quit")) running = 0;
            if (test && ev.type == GOOME_KEY && !p.paused) {
                static const int mod_bits[4] = {GOOME_SHIFT, GOOME_CTRL, GOOME_ALT, GOOME_SUPER};
                for (int i = 0; i < 4; i++)
                    if (ev.mods & mod_bits[i]) key_down(&m, &p, K_SHIFT + i, now);
                int code = ev.key;
                if (code >= 'A' && code <= 'Z') code += 'a' - 'A';
                key_down(&m, &p, code, now);
                for (int i = 0; i < 4; i++)
                    if (ev.mods & mod_bits[i]) key_up(&m, &p, K_SHIFT + i, now);
            }
            if (test && (ev.type == GOOME_DOWN || ev.type == GOOME_UP) && !p.paused) {
                int bit = ev.button == 1 ? 1 : ev.button == 2 ? 2 : 4;
                buttons = ev.type == GOOME_DOWN ? buttons | bit : buttons & ~bit;
                mouse_at = now;
                if (ev.type == GOOME_DOWN && p.clicks)
                    for (int i = 0; i < MAX_RIPPLES; i++)
                        if (now - ripples[i].born > 600) {
                            ripples[i] = (ripple){ev.x, ev.y, bit, now};
                            break;
                        }
            }
            goome_ui_feed(u, &ev);
        }
        if (!running) break;
        now = goome_now_ms();
        keyev kev[64];
        int nk = src_poll(kev, 64);
        for (int i = 0; i < nk && !p.paused; i++) {
            if (kev[i].down) key_down(&m, &p, kev[i].code, now);
            else key_up(&m, &p, kev[i].code, now);
        }
        if (!test) {
            int b = 0, have = src_buttons(&b);
            double px = 0, py = 0;
            int pb = 0, have_ptr = goome_pointer(g, &px, &py, &pb) == 1;
            if (!have && have_ptr) b = pb, have = 1;
            int sc = src_scroll();
            if (sc && !p.paused) scroll = sc, mouse_at = now;
            if (have && b != buttons && !p.paused) {
                int pressed = b & ~buttons;
                mouse_at = now;
                if (pressed && have_ptr && p.clicks)
                    for (int i = 0; i < MAX_RIPPLES; i++)
                        if (now - ripples[i].born > 600) {
                            ripples[i] = (ripple){px, py, pressed & 1 ? 1 : pressed & 4 ? 4 : 2, now};
                            break;
                        }
            }
            buttons = b;
        }
        if (prefs_dirty && !settings) save_prefs(cfg, &p), prefs_dirty = 0;

        int alive = settings || (status_until && now < status_until);
        long fade_ms = (long)(p.fade * 1000);
        for (int i = 0; i < nchips; i++)
            if (now - chips[i].touched < fade_ms + 300) alive = 1;
        while (nchips && now - chips[0].touched >= fade_ms + 300) memmove(chips, chips + 1, sizeof(chip) * (size_t)(--nchips));
        for (int i = 0; i < MAX_RIPPLES; i++)
            if (ripples[i].born && now - ripples[i].born < 500) alive = 1;
        if (p.mouse && now - mouse_at < fade_ms + 300) alive = 1;
        if (!alive && !drawn && !test) continue;
        drawn = alive;
        if (nmons < 1) continue;

        double k = p.size;
        goome_rect a = mons[p.mon].area;
        for (int mi = 0; mi < nmons && mi < MAX_MONS; mi++) {
            if (mi == p.mon) continue;
            goome_canvas *oc = goome_begin(g, mi);
            if (!oc) continue;
            goome_clear(oc);
            for (int i = 0; i < MAX_RIPPLES; i++) {
                double t = (now - ripples[i].born) / 500.0;
                if (!ripples[i].born || t >= 1) continue;
                uint32_t col = ripples[i].button == 1 ? 0xFF6FDC5F : ripples[i].button == 4 ? 0xFF4FC3FF : 0xFFF0C24F;
                goome_draw_ring(oc, ripples[i].x, ripples[i].y, 8 + 26 * ease_out(t), 3 * (1 - t) + 1, alpha(col, 1 - t));
            }
            goome_end(g, oc, NULL, 0);
        }
        goome_canvas *c = goome_ui_begin(u, p.mon);
        if (!c) continue;
        double margin = 48 * k, gap = 10 * k, chip_h = 50 * k;
        int top = p.spot >= TOP_CENTER, col = p.spot % 3;
        double y = top ? a.y + margin : a.y + a.h - margin - chip_h;
        double mouse_w = p.mouse && now - mouse_at < fade_ms + 300 ? 34 * k + gap : 0;
        for (int i = nchips - 1; i >= 0; i--) {
            chip *ch = &chips[i];
            double age = (double)(now - ch->touched), t_in = ease_out((now - ch->born) / 180.0);
            double fa = age < fade_ms ? 1 : 1 - (age - fade_ms) / 300.0;
            if (fa <= 0) continue;
            double w = chip_width(font, k, ch);
            double mw = i == nchips - 1 ? mouse_w : 0;
            double x = col == 0 ? a.x + (a.w - w + mw) / 2 : col == 1 ? a.x + margin + mw : a.x + a.w - margin - w;
            double slide = (1 - t_in) * 14 * k * (top ? -1 : 1);
            draw_chip(c, font, k, ch, x, y + slide, fa * t_in);
            y += top ? chip_h + gap : -(chip_h + gap);
        }
        if (mouse_w > 0) {
            double age = (double)(now - mouse_at), fa = age < fade_ms ? 1 : 1 - (age - fade_ms) / 300.0;
            double lw = nchips ? chip_width(font, k, &chips[nchips - 1]) : 0;
            double by = top ? a.y + margin : a.y + a.h - margin - chip_h;
            double bx = col == 0 ? a.x + (a.w - lw - mouse_w) / 2 : col == 1 ? a.x + margin : a.x + a.w - margin - lw - mouse_w;
            if (!nchips && col == 0) bx = a.x + (a.w - 34 * k) / 2;
            if (!nchips && col == 2) bx = a.x + a.w - margin - 34 * k;
            draw_mouse(c, k, bx, by, buttons, now - mouse_at < 400 ? scroll : 0, fa);
        }
        for (int i = 0; i < MAX_RIPPLES; i++) {
            double t = (now - ripples[i].born) / 500.0;
            if (!ripples[i].born || t >= 1) continue;
            uint32_t rc = ripples[i].button == 1 ? 0xFF6FDC5F : ripples[i].button == 4 ? 0xFF4FC3FF : 0xFFF0C24F;
            goome_draw_ring(c, ripples[i].x, ripples[i].y, 8 + 26 * ease_out(t), 3 * (1 - t) + 1, alpha(rc, 1 - t));
        }
        if (status_until && now < status_until && !settings) {
            double sw = goome_text_measure(font, 14, status, NULL) + 28;
            double sx = a.x + (a.w - sw) / 2, sy = a.y + 24;
            goome_draw_rrect(c, sx, sy, sw, 36, goome_radius(10), 0xE6121418);
            goome_draw_rrect_line(c, sx + 0.5, sy + 0.5, sw - 1, 35, goome_radius(10), 1, 0x66F0C24F);
            goome_text(c, font, 14, sx + 14, sy + 9, status, 0xFFF0C24F);
        }
        if (settings) {
            int open = 1, ch = 0;
            if (goome_ui_window(u, "keyviz", &wx, &wy, &ww, &wh, GOOME_WIN_MOVE | GOOME_WIN_CLOSE, &open)) {
                ch |= goome_ui_combo(u, "Where", &p.spot, spot_names, SPOTS);
                ch |= goome_ui_slider(u, "Size", &p.size, 0.6, 2.5, "%.2fx");
                ch |= goome_ui_slider(u, "Stays for", &p.fade, 0.5, 8, "%.1f s");
                ch |= goome_ui_toggle(u, "Show typing", &p.typing);
                goome_ui_tooltip(u, "off = only shortcuts like Ctrl + C show up, handy when you type passwords");
                ch |= goome_ui_toggle(u, "Click circles", &p.clicks);
                ch |= goome_ui_toggle(u, "Mouse buttons", &p.mouse);
                goome_ui_toggle(u, "Paused", &p.paused);
                if (nmons > 1) {
                    const char *names[MAX_MONS];
                    char labels[MAX_MONS][80];
                    for (int i = 0; i < nmons && i < MAX_MONS; i++) {
                        snprintf(labels[i], sizeof(labels[i]), "%d: %.40s (%dx%d)", i + 1, mons[i].name, mons[i].area.w, mons[i].area.h);
                        names[i] = labels[i];
                    }
                    ch |= goome_ui_combo(u, "Monitor", &p.mon, names, nmons < MAX_MONS ? nmons : MAX_MONS);
                }
                if (status[0]) {
                    goome_ui_separator(u);
                    goome_ui_wrapped(u, status);
                }
                goome_ui_separator(u);
                goome_ui_wrapped(u, "alt+shift+k opens this, alt+shift+p pauses. nothing here takes clicks except this window.");
                goome_ui_row(u, 2);
                if (goome_ui_button(u, "Done")) open = 0;
                if (goome_ui_button(u, "Quit")) running = 0;
            }
            goome_ui_window_end(u);
            if (ch) prefs_dirty = 1;
            if (!open) settings = 0;
        }
        goome_ui_end(u);
    }
    save_prefs(cfg, &p);
    goome_ui_free(u);
    goome_close(g);
    return 0;
}
