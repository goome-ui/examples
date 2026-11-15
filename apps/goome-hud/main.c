#define _DEFAULT_SOURCE
#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#define _CRT_SECURE_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <direct.h>
#elif defined(__APPLE__)
#include <ifaddrs.h>
#include <mach/mach.h>
#include <net/if.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "goome.h"

#define HISTORY 60
#define MAX_MONS 8

enum { HOTKEY_HIDE = 1, HOTKEY_SIZE };

typedef struct {
    double cpu, ram_used, ram_total, down, up;
    double cpu_temp, gpu_temp;
    int have_cpu, have_ram, have_net, have_cpu_temp, have_gpu_temp;
} stats;

typedef struct {
    unsigned long long cpu_idle, cpu_total, rx, tx;
    long net_at;
    int primed;
} sampler;

static const uint32_t PANEL = 0xE6101318, EDGE = 0x24FFFFFF, TEXT = 0xFFE9ECF1, DIM = 0xFF8B94A5;
static const uint32_t GREEN = 0xFF6FDC5F, BLUE = 0xFF4FC3FF, AMBER = 0xFFF0C24F, RED = 0xFFFF6B6B, VIOLET = 0xFFB18CFF;

#if defined(_WIN32)

static unsigned long long ft64(FILETIME f) { return (unsigned long long)f.dwHighDateTime << 32 | f.dwLowDateTime; }

static int read_cpu(unsigned long long *idle, unsigned long long *total)
{
    FILETIME i, k, u;
    if (!GetSystemTimes(&i, &k, &u)) return 0;
    *idle = ft64(i), *total = ft64(k) + ft64(u);
    return 1;
}

static int read_ram(double *used, double *total)
{
    MEMORYSTATUSEX m;
    m.dwLength = sizeof(m);
    if (!GlobalMemoryStatusEx(&m)) return 0;
    *total = (double)m.ullTotalPhys, *used = (double)(m.ullTotalPhys - m.ullAvailPhys);
    return 1;
}

typedef DWORD(WINAPI *if_table_fn)(PMIB_IF_TABLE2 *);
typedef void(WINAPI *free_table_fn)(PVOID);

static int read_net(unsigned long long *rx, unsigned long long *tx)
{
    static HMODULE lib;
    static if_table_fn get_table;
    static free_table_fn free_table;
    if (!lib) {
        lib = LoadLibraryA("iphlpapi.dll");
        if (!lib) return 0;
        get_table = (if_table_fn)(void (*)(void))GetProcAddress(lib, "GetIfTable2");
        free_table = (free_table_fn)(void (*)(void))GetProcAddress(lib, "FreeMibTable");
    }
    if (!get_table || !free_table) return 0;
    PMIB_IF_TABLE2 t = NULL;
    if (get_table(&t) != NO_ERROR || !t) return 0;
    *rx = *tx = 0;
    for (ULONG i = 0; i < t->NumEntries; i++) {
        MIB_IF_ROW2 *r = &t->Table[i];
        if (r->Type == IF_TYPE_SOFTWARE_LOOPBACK || r->OperStatus != IfOperStatusUp) continue;
        if (r->InterfaceAndOperStatusFlags.FilterInterface || !r->InterfaceAndOperStatusFlags.HardwareInterface) continue;
        *rx += r->InOctets, *tx += r->OutOctets;
    }
    free_table(t);
    return 1;
}

static int read_temps(stats *s) { return 0; }

#elif defined(__APPLE__)

static int read_cpu(unsigned long long *idle, unsigned long long *total)
{
    host_cpu_load_info_data_t info;
    mach_msg_type_number_t n = HOST_CPU_LOAD_INFO_COUNT;
    if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, (host_info_t)&info, &n) != KERN_SUCCESS) return 0;
    *idle = info.cpu_ticks[CPU_STATE_IDLE];
    *total = 0;
    for (int i = 0; i < CPU_STATE_MAX; i++) *total += info.cpu_ticks[i];
    return 1;
}

static int read_ram(double *used, double *total)
{
    unsigned long long mem = 0;
    size_t len = sizeof(mem);
    if (sysctlbyname("hw.memsize", &mem, &len, NULL, 0) != 0) return 0;
    vm_statistics64_data_t vm;
    mach_msg_type_number_t n = HOST_VM_INFO64_COUNT;
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vm, &n) != KERN_SUCCESS) return 0;
    vm_size_t page = 0;
    host_page_size(mach_host_self(), &page);
    double pages = (double)vm.internal_page_count - (double)vm.purgeable_count + vm.wire_count + vm.compressor_page_count;
    *total = (double)mem, *used = pages * (double)page;
    return 1;
}

static int read_net(unsigned long long *rx, unsigned long long *tx)
{
    struct ifaddrs *list;
    if (getifaddrs(&list) != 0) return 0;
    *rx = *tx = 0;
    for (struct ifaddrs *a = list; a; a = a->ifa_next) {
        if (!a->ifa_addr || a->ifa_addr->sa_family != AF_LINK || !a->ifa_data) continue;
        if (a->ifa_flags & IFF_LOOPBACK) continue;
        struct if_data *d = a->ifa_data;
        *rx += d->ifi_ibytes, *tx += d->ifi_obytes;
    }
    freeifaddrs(list);
    return 1;
}

static int read_temps(stats *s) { return 0; }

#else

static int read_cpu(unsigned long long *idle, unsigned long long *total)
{
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return 0;
    unsigned long long v[8] = {0};
    int n = fscanf(f, "cpu %llu %llu %llu %llu %llu %llu %llu %llu", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7]);
    fclose(f);
    if (n < 4) return 0;
    *idle = v[3] + v[4];
    *total = 0;
    for (int i = 0; i < 8; i++) *total += v[i];
    return 1;
}

static int read_ram(double *used, double *total)
{
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return 0;
    char line[128];
    double t = 0, avail = -1;
    while (fgets(line, sizeof(line), f)) {
        unsigned long long kb;
        if (sscanf(line, "MemTotal: %llu", &kb) == 1) t = kb * 1024.0;
        if (sscanf(line, "MemAvailable: %llu", &kb) == 1) avail = kb * 1024.0;
    }
    fclose(f);
    if (t <= 0 || avail < 0) return 0;
    *total = t, *used = t - avail;
    return 1;
}

static int read_net(unsigned long long *rx, unsigned long long *tx)
{
    FILE *f = fopen("/proc/net/dev", "r");
    if (!f) return 0;
    char line[512];
    *rx = *tx = 0;
    while (fgets(line, sizeof(line), f)) {
        char *colon = strchr(line, ':');
        if (!colon) continue;
        *colon = 0;
        char *name = line;
        while (*name == ' ') name++;
        if (!strcmp(name, "lo") || !strncmp(name, "veth", 4) || !strncmp(name, "docker", 6) || !strncmp(name, "br-", 3) || !strncmp(name, "virbr", 5)) continue;
        unsigned long long v[9];
        if (sscanf(colon + 1, "%llu %llu %llu %llu %llu %llu %llu %llu %llu", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7], &v[8]) == 9)
            *rx += v[0], *tx += v[8];
    }
    fclose(f);
    return 1;
}

static int read_number(const char *path, double *out)
{
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    long v;
    int ok = fscanf(f, "%ld", &v) == 1;
    fclose(f);
    if (ok) *out = v / 1000.0;
    return ok;
}

static int read_temps(stats *s)
{
    static const char *const cpu_chips[] = {"k10temp", "zenpower", "coretemp", "cpu_thermal", "soc_thermal", "acpitz"};
    static char cpu_path[256], gpu_path[256];
    static int looked;
    if (!looked) {
        looked = 1;
        int best = 99;
        DIR *d = opendir("/sys/class/hwmon");
        struct dirent *e;
        while (d && (e = readdir(d))) {
            if (e->d_name[0] == '.') continue;
            char path[300], name[64] = "";
            snprintf(path, sizeof(path), "/sys/class/hwmon/%.200s/name", e->d_name);
            FILE *f = fopen(path, "r");
            if (!f) continue;
            if (!fgets(name, sizeof(name), f)) name[0] = 0;
            fclose(f);
            name[strcspn(name, "\n")] = 0;
            for (int i = 0; i < (int)(sizeof(cpu_chips) / sizeof(*cpu_chips)); i++)
                if (!strcmp(name, cpu_chips[i]) && i < best) {
                    best = i;
                    snprintf(cpu_path, sizeof(cpu_path), "/sys/class/hwmon/%.200s/temp1_input", e->d_name);
                }
            if (!gpu_path[0] && (!strcmp(name, "amdgpu") || !strcmp(name, "nouveau") || !strcmp(name, "i915") || !strcmp(name, "xe")))
                snprintf(gpu_path, sizeof(gpu_path), "/sys/class/hwmon/%.200s/temp1_input", e->d_name);
        }
        if (d) closedir(d);
        if (!cpu_path[0]) snprintf(cpu_path, sizeof(cpu_path), "/sys/class/thermal/thermal_zone0/temp");
    }
    s->have_cpu_temp = read_number(cpu_path, &s->cpu_temp) && s->cpu_temp > 0;
    s->have_gpu_temp = gpu_path[0] && read_number(gpu_path, &s->gpu_temp) && s->gpu_temp > 0;
    return s->have_cpu_temp || s->have_gpu_temp;
}

#endif

static void sample(sampler *sm, stats *s)
{
    unsigned long long idle, total, rx, tx;
    if (read_cpu(&idle, &total)) {
        if (sm->primed && total > sm->cpu_total) {
            s->cpu = 1.0 - (double)(idle - sm->cpu_idle) / (double)(total - sm->cpu_total);
            if (s->cpu < 0) s->cpu = 0;
            if (s->cpu > 1) s->cpu = 1;
            s->have_cpu = 1;
        }
        sm->cpu_idle = idle, sm->cpu_total = total;
    }
    s->have_ram = read_ram(&s->ram_used, &s->ram_total);
    long now = goome_now_ms();
    if (read_net(&rx, &tx)) {
        double secs = (now - sm->net_at) / 1000.0;
        if (sm->primed && secs > 0.05 && rx >= sm->rx && tx >= sm->tx) {
            s->down = (double)(rx - sm->rx) / secs, s->up = (double)(tx - sm->tx) / secs;
            s->have_net = 1;
        }
        sm->rx = rx, sm->tx = tx;
    }
    sm->net_at = now;
    read_temps(s);
    sm->primed = 1;
}

static void config_dir(char *out, int cap)
{
#ifdef _WIN32
    const char *base = getenv("APPDATA");
    snprintf(out, cap, "%s\\goome-hud", base ? base : ".");
    _mkdir(out);
#elif defined(__APPLE__)
    const char *home = getenv("HOME");
    snprintf(out, cap, "%s/Library/Application Support/goome-hud", home ? home : ".");
    mkdir(out, 0700);
#else
    const char *xdg = getenv("XDG_CONFIG_HOME"), *home = getenv("HOME");
    if (xdg && *xdg) snprintf(out, cap, "%s/goome-hud", xdg);
    else snprintf(out, cap, "%s/.config/goome-hud", home ? home : ".");
    mkdir(out, 0700);
#endif
}

static void rate_text(char *out, int cap, double bytes)
{
    if (bytes >= 1e9) snprintf(out, cap, "%.1f GB/s", bytes / 1e9);
    else if (bytes >= 1e6) snprintf(out, cap, "%.1f MB/s", bytes / 1e6);
    else if (bytes >= 1e3) snprintf(out, cap, "%.0f KB/s", bytes / 1e3);
    else snprintf(out, cap, "%.0f B/s", bytes);
}

static void short_rate(char *out, int cap, double bytes)
{
    if (bytes >= 1e9) snprintf(out, cap, "%.1fG", bytes / 1e9);
    else if (bytes >= 1e6) snprintf(out, cap, "%.1fM", bytes / 1e6);
    else snprintf(out, cap, "%.0fK", bytes / 1e3);
}

static uint32_t heat(double frac)
{
    return frac < 0.6 ? GREEN : frac < 0.85 ? AMBER : RED;
}

static uint32_t temp_color(double c) { return c < 65 ? GREEN : c < 82 ? AMBER : RED; }

static uint32_t fade(uint32_t c, double a)
{
    return (uint32_t)(((c >> 24) & 0xFF) * a) << 24 | (c & 0xFFFFFF);
}

static void graph(goome_canvas *c, double x, double y, double w, double h, const float *v, int at, double top, uint32_t col)
{
    double pts[(HISTORY + 2) * 2];
    for (int i = 0; i < HISTORY; i++) {
        double val = v[(at + i) % HISTORY];
        if (val > top) val = top;
        pts[i * 2] = x + w * i / (HISTORY - 1);
        pts[i * 2 + 1] = y + h - h * val / top;
    }
    pts[HISTORY * 2] = x + w, pts[HISTORY * 2 + 1] = y + h;
    pts[HISTORY * 2 + 2] = x, pts[HISTORY * 2 + 3] = y + h;
    goome_draw_poly(c, pts, HISTORY + 2, fade(col, 0.18));
    for (int i = 1; i < HISTORY; i++) goome_draw_line(c, pts[i * 2 - 2], pts[i * 2 - 1], pts[i * 2], pts[i * 2 + 1], 1.5, col);
}

static double peak(const float *v, double floor)
{
    double m = floor;
    for (int i = 0; i < HISTORY; i++)
        if (v[i] > m) m = v[i];
    return m * 1.15;
}

static void arrow(goome_canvas *c, double x, double y, int down, uint32_t col)
{
    if (down) goome_draw_triangle(c, x, y + 2, x + 8, y + 2, x + 4, y + 8, col);
    else goome_draw_triangle(c, x, y + 8, x + 8, y + 8, x + 4, y + 2, col);
}

typedef struct {
    double x, y;
    int compact, hidden;
} layout;

static void hud_size(const layout *l, const stats *s, double *w, double *h)
{
    *w = l->compact ? (s->have_cpu_temp ? 330 : 286) : 236;
    *h = l->compact ? 44 : 22 + 58 + 50 + 58 + (s->have_cpu_temp || s->have_gpu_temp ? 30 : 0) + 6;
}

static void draw_hud(goome_canvas *c, goome_font *f, const layout *l, const stats *s, const float *cpu_h, const float *down_h, const float *up_h, int at, int handle_hot)
{
    double w, h, x = l->x, y = l->y;
    hud_size(l, s, &w, &h);
    goome_draw_shadow(c, x, y + 4, w, h, 12, 18, 0x66000000);
    goome_draw_rrect(c, x, y, w, h, goome_radius(12), PANEL);
    goome_draw_rrect_line(c, x + 0.5, y + 0.5, w - 1, h - 1, goome_radius(12), 1, EDGE);
    char buf[64], buf2[64];
    if (l->compact) {
        double hx = x + 6, hy = y + 10;
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 2; j++) goome_draw_circle(c, hx + 4 + j * 6, hy + 4 + i * 8, 1.6, handle_hot ? TEXT : DIM);
        double cx = x + 30;
        snprintf(buf, sizeof(buf), "%.0f%%", s->cpu * 100);
        cx += goome_text(c, f, 11, cx, y + 15, "CPU", DIM) + 5;
        cx += goome_text(c, f, 14, cx, y + 13, s->have_cpu ? buf : "-", heat(s->cpu)) + 14;
        double rf = s->have_ram ? s->ram_used / s->ram_total : 0;
        snprintf(buf, sizeof(buf), "%.0f%%", rf * 100);
        cx += goome_text(c, f, 11, cx, y + 15, "RAM", DIM) + 5;
        cx += goome_text(c, f, 14, cx, y + 13, s->have_ram ? buf : "-", heat(rf)) + 14;
        short_rate(buf, sizeof(buf), s->down);
        short_rate(buf2, sizeof(buf2), s->up);
        arrow(c, cx, y + 16, 1, GREEN);
        cx += 11;
        cx += goome_text(c, f, 13, cx, y + 14, buf, TEXT) + 6;
        arrow(c, cx, y + 16, 0, BLUE);
        cx += 11;
        cx += goome_text(c, f, 13, cx, y + 14, buf2, TEXT) + 14;
        if (s->have_cpu_temp) {
            snprintf(buf, sizeof(buf), "%.0f\xC2\xB0", s->cpu_temp);
            goome_text(c, f, 14, cx, y + 13, buf, temp_color(s->cpu_temp));
        }
        return;
    }
    for (int i = 0; i < 3; i++) goome_draw_circle(c, x + w / 2 - 8 + i * 8, y + 11, 1.8, handle_hot ? TEXT : DIM);
    double px = x + 14, pw = w - 28, cy = y + 22;

    goome_text(c, f, 11, px, cy, "CPU", DIM);
    snprintf(buf, sizeof(buf), s->have_cpu ? "%.0f%%" : "-", s->cpu * 100);
    goome_text(c, f, 16, px + pw - goome_text_measure(f, 16, buf, NULL), cy - 3, buf, heat(s->cpu));
    graph(c, px, cy + 18, pw, 32, cpu_h, at, 1.0, heat(s->cpu));
    cy += 58;

    goome_text(c, f, 11, px, cy, "RAM", DIM);
    double rf = s->have_ram && s->ram_total > 0 ? s->ram_used / s->ram_total : 0;
    if (s->have_ram) snprintf(buf, sizeof(buf), "%.1f / %.1f GB", s->ram_used / 1073741824.0, s->ram_total / 1073741824.0);
    else snprintf(buf, sizeof(buf), "-");
    goome_text(c, f, 13, px + pw - goome_text_measure(f, 13, buf, NULL), cy - 1, buf, TEXT);
    goome_draw_rrect(c, px, cy + 22, pw, 8, goome_radius(4), 0x1FFFFFFF);
    if (rf > 0) goome_draw_rrect_grad(c, px, cy + 22, pw * rf < 8 ? 8 : pw * rf, 8, goome_radius(4), fade(heat(rf), 0.75), heat(rf), 1);
    cy += 50;

    goome_text(c, f, 11, px, cy, "NET", DIM);
    rate_text(buf, sizeof(buf), s->down);
    rate_text(buf2, sizeof(buf2), s->up);
    double uw = goome_text_measure(f, 12, buf2, NULL), dw = goome_text_measure(f, 12, buf, NULL);
    double ux = px + pw - uw, dx = ux - 22 - dw;
    arrow(c, dx - 11, cy + 2, 1, GREEN);
    goome_text(c, f, 12, dx, cy, buf, TEXT);
    arrow(c, ux - 11, cy + 2, 0, BLUE);
    goome_text(c, f, 12, ux, cy, buf2, TEXT);
    double top = peak(down_h, 1024) > peak(up_h, 1024) ? peak(down_h, 1024) : peak(up_h, 1024);
    graph(c, px, cy + 18, pw, 32, up_h, at, top, BLUE);
    graph(c, px, cy + 18, pw, 32, down_h, at, top, GREEN);
    cy += 58;

    if (s->have_cpu_temp || s->have_gpu_temp) {
        goome_text(c, f, 11, px, cy + 2, "TEMP", DIM);
        double tx = px + pw;
        if (s->have_gpu_temp) {
            snprintf(buf, sizeof(buf), "%.0f\xC2\xB0", s->gpu_temp);
            tx -= goome_text_measure(f, 15, buf, NULL);
            goome_text(c, f, 15, tx, cy - 1, buf, temp_color(s->gpu_temp));
            tx -= goome_text_measure(f, 11, "GPU", NULL) + 5;
            goome_text(c, f, 11, tx, cy + 2, "GPU", DIM);
            tx -= 16;
        }
        if (s->have_cpu_temp) {
            snprintf(buf, sizeof(buf), "%.0f\xC2\xB0", s->cpu_temp);
            tx -= goome_text_measure(f, 15, buf, NULL);
            goome_text(c, f, 15, tx, cy - 1, buf, temp_color(s->cpu_temp));
            tx -= goome_text_measure(f, 11, "CPU", NULL) + 5;
            goome_text(c, f, 11, tx, cy + 2, "CPU", VIOLET);
        }
    }
}

static goome_rect handle_rect(const layout *l, const stats *s)
{
    double w, h;
    hud_size(l, s, &w, &h);
    if (l->compact) return (goome_rect){(int)l->x, (int)l->y, 26, (int)h};
    return (goome_rect){(int)l->x, (int)l->y, (int)w, 22};
}

static void load_layout(const char *path, layout *l)
{
    FILE *f = fopen(path, "r");
    if (!f) return;
    double x, y;
    int compact;
    if (fscanf(f, "%lf %lf %d", &x, &y, &compact) == 3) l->x = x, l->y = y, l->compact = !!compact;
    fclose(f);
}

static void save_layout(const char *path, const layout *l)
{
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "%.0f %.0f %d\n", l->x, l->y, l->compact);
    fclose(f);
}

static void snap(layout *l, const stats *s, const goome_monitor *mons, int n)
{
    double w, h;
    hud_size(l, s, &w, &h);
    double cx = l->x + w / 2, cy = l->y + h / 2;
    int best = 0;
    double best_d = 1e18;
    for (int i = 0; i < n; i++) {
        goome_rect a = mons[i].area;
        double dx = cx < a.x ? a.x - cx : cx > a.x + a.w ? cx - a.x - a.w : 0;
        double dy = cy < a.y ? a.y - cy : cy > a.y + a.h ? cy - a.y - a.h : 0;
        if (dx * dx + dy * dy < best_d) best_d = dx * dx + dy * dy, best = i;
    }
    if (n < 1) return;
    goome_rect a = mons[best].area;
    double m = 14, pull = 28;
    if (l->x < a.x + m + pull) l->x = a.x + m;
    if (l->y < a.y + m + pull) l->y = a.y + m;
    if (l->x + w > a.x + a.w - m - pull) l->x = a.x + a.w - m - w;
    if (l->y + h > a.y + a.h - m - pull) l->y = a.y + a.h - m - h;
}

int main(int argc, char **argv)
{
    char dir[1024], cfg[1100];
    config_dir(dir, sizeof(dir));
    if (argc >= 3 && !strcmp(argv[1], "--send")) {
        int rc = goome_send(dir, argv[2]);
        if (rc != GOOME_OK) fprintf(stderr, "goome-hud: %s, is it running?\n", goome_strerror(rc));
        return rc != GOOME_OK;
    }
    int err;
    goome *g = goome_open(&(goome_opts){.title = "goome-hud", .no_capture = 1, .state_dir = dir}, &err);
    if (!g) {
        fprintf(stderr, "goome-hud: cant open an overlay here, %s\n", goome_strerror(err));
        return 1;
    }
    int test = !strcmp(goome_backend_name(g), "test");
    snprintf(cfg, sizeof(cfg), test ? "hud.txt" : "%s/hud.txt", dir);
    goome_font *font = goome_font_system(NULL);
    goome_hotkey(g, "alt+shift+h", HOTKEY_HIDE);
    goome_hotkey(g, "alt+shift+j", HOTKEY_SIZE);

    goome_monitor mons[MAX_MONS];
    int nmons = goome_monitors(g, mons, MAX_MONS);
    stats s = {0};
    sampler sm = {0};
    sample(&sm, &s);
    layout l = {0};
    double w, h;
    hud_size(&l, &s, &w, &h);
    if (nmons > 0) l.x = mons[0].area.x + mons[0].area.w - w - 14, l.y = mons[0].area.y + 14;
    load_layout(cfg, &l);
    snap(&l, &s, mons, nmons);

    float cpu_h[HISTORY] = {0}, down_h[HISTORY] = {0}, up_h[HISTORY] = {0};
    int at = 0, running = 1, dragging = 0, hot = 0, dirty = 1;
    double grab_x = 0, grab_y = 0;
    long next_sample = goome_now_ms() + (test ? 100 : 1000);
    int drawn_on[MAX_MONS] = {0};
    while (running) {
        long now = goome_now_ms();
        int wait = dragging || test ? 16 : (int)(next_sample - now);
        if (wait < 0) wait = 0;
        goome_event ev;
        while (goome_poll(g, &ev, wait) > 0) {
            wait = 0;
            goome_rect hr = handle_rect(&l, &s);
            int on_handle = ev.x >= hr.x && ev.x < hr.x + hr.w && ev.y >= hr.y && ev.y < hr.y + hr.h;
            switch (ev.type) {
            case GOOME_QUIT: running = 0; break;
            case GOOME_MONITORS:
                nmons = goome_monitors(g, mons, MAX_MONS);
                snap(&l, &s, mons, nmons);
                memset(drawn_on, 0, sizeof(drawn_on));
                dirty = 1;
                break;
            case GOOME_HOTKEY:
                if (ev.id == HOTKEY_HIDE) l.hidden = !l.hidden;
                if (ev.id == HOTKEY_SIZE) l.compact = !l.compact, snap(&l, &s, mons, nmons), save_layout(cfg, &l);
                dirty = 1;
                break;
            case GOOME_CONTROL:
                if (!strcmp(ev.msg, "hide")) l.hidden = !l.hidden;
                if (!strcmp(ev.msg, "size")) l.compact = !l.compact, snap(&l, &s, mons, nmons), save_layout(cfg, &l);
                if (!strcmp(ev.msg, "quit")) running = 0;
                dirty = 1;
                break;
            case GOOME_DOWN:
                if (on_handle && ev.button == 1) {
                    dragging = 1, grab_x = ev.x - l.x, grab_y = ev.y - l.y;
                    goome_grab(g, 1);
                    goome_cursor(g, GOOME_CURSOR_MOVE);
                } else if (on_handle && ev.button == 3) {
                    l.compact = !l.compact;
                    snap(&l, &s, mons, nmons);
                    save_layout(cfg, &l);
                    dirty = 1;
                }
                break;
            case GOOME_MOTION:
                if (dragging) l.x = ev.x - grab_x, l.y = ev.y - grab_y, dirty = 1;
                else if (on_handle != hot) hot = on_handle, dirty = 1, goome_cursor(g, hot ? GOOME_CURSOR_HAND : GOOME_CURSOR_ARROW);
                break;
            case GOOME_UP:
                if (dragging && ev.button == 1) {
                    dragging = 0;
                    goome_grab(g, 0);
                    snap(&l, &s, mons, nmons);
                    save_layout(cfg, &l);
                    dirty = 1;
                }
                break;
            }
        }
        if (!running) break;
        now = goome_now_ms();
        if (now >= next_sample) {
            sample(&sm, &s);
            cpu_h[at] = (float)s.cpu, down_h[at] = (float)s.down, up_h[at] = (float)s.up;
            at = (at + 1) % HISTORY;
            next_sample = now + (test ? 100 : 1000);
            dirty = 1;
        }
        if (!dirty) continue;
        dirty = 0;
        hud_size(&l, &s, &w, &h);
        for (int i = 0; i < nmons && i < MAX_MONS; i++) {
            goome_rect a = mons[i].area;
            int on = !l.hidden && l.x < a.x + a.w && l.x + w > a.x && l.y < a.y + a.h && l.y + h > a.y;
            if (!on && !drawn_on[i]) continue;
            goome_canvas *c = goome_begin(g, i);
            if (!c) continue;
            goome_clear(c);
            goome_rect hr = handle_rect(&l, &s);
            if (on) draw_hud(c, font, &l, &s, cpu_h, down_h, up_h, at, hot || dragging);
            goome_end(g, c, on ? &hr : NULL, on ? 1 : 0);
            drawn_on[i] = on;
        }
    }
    save_layout(cfg, &l);
    goome_font_free(font);
    goome_close(g);
    return 0;
}
