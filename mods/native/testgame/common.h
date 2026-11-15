#ifndef TG_COMMON_H
#define TG_COMMON_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <winver.h>

enum { TG_W = 480, TG_H = 360, MENU_X = 40, MENU_Y = 40, MENU_W = 300 };

static int tg_input;
static HWND tg_win;
static unsigned char tg_clear[3] = {20, 28, 36};

static LRESULT CALLBACK tg_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if ((m >= WM_MOUSEFIRST && m <= WM_MOUSELAST) || (m >= WM_KEYFIRST && m <= WM_KEYLAST) || m == WM_INPUT) tg_input++;
    return DefWindowProcW(h, m, w, l);
}

static HWND tg_make_window(const wchar_t *cls)
{
    DWORD vh = 0;
    GetFileVersionInfoSizeW(L"kernel32.dll", &vh);
    WNDCLASSW wc;
    memset(&wc, 0, sizeof(wc));
    wc.style = CS_OWNDC, wc.lpfnWndProc = tg_proc, wc.hInstance = GetModuleHandleW(NULL), wc.lpszClassName = cls;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);
    RECT r = {0, 0, TG_W, TG_H};
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    HWND h = CreateWindowW(cls, L"goome mod test game", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, r.right - r.left, r.bottom - r.top, NULL, NULL, GetModuleHandleW(NULL), NULL);
    RECT c;
    if (h && GetClientRect(h, &c) && (c.right != TG_W || c.bottom != TG_H))
        SetWindowPos(h, NULL, 0, 0, r.right - r.left + TG_W - c.right, r.bottom - r.top + TG_H - c.bottom, SWP_NOMOVE | SWP_NOZORDER);
    RAWINPUTDEVICE rid = {1, 2, 0, h};
    RegisterRawInputDevices(&rid, 1, sizeof(rid));
    tg_win = h;
    return h;
}

static void tg_pump(void)
{
    MSG m;
    while (PeekMessageW(&m, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
}

static int tg_send(UINT msg, WPARAM w, int x, int y)
{
    int before = tg_input;
    SendMessageW(tg_win, msg, w, MAKELPARAM(x, y));
    return tg_input - before;
}

static int tg_eaten_over_menu(void)
{
    return tg_send(WM_MOUSEMOVE, 0, MENU_X + 80, MENU_Y + 40) == 0;
}

typedef void (*tg_frame)(void);
typedef int (*tg_readback)(unsigned char *rgba);

static int tg_menu_ink(tg_readback read)
{
    static unsigned char buf[TG_W * TG_H * 4];
    if (!read(buf)) return -1;
    int ink = 0;
    for (int y = MENU_Y + 2; y < MENU_Y + 24; y++)
        for (int x = MENU_X + 4; x < MENU_X + MENU_W - 4; x++) {
            const unsigned char *p = buf + ((size_t)y * TG_W + x) * 4;
            int d = abs(p[0] - tg_clear[0]) + abs(p[1] - tg_clear[1]) + abs(p[2] - tg_clear[2]);
            if (d > 40) ink++;
        }
    return ink;
}

static int tg_run(const char *api, tg_frame frame, tg_readback read)
{
    int fails = 0, warm = 0;
    printf("%s mod in the test game\n", api);
    for (warm = 0; warm < 400; warm++) {
        tg_pump();
        frame();
        if (tg_eaten_over_menu()) break;
        Sleep(5);
    }
    if (warm >= 400) {
        printf("  the mod never took the window input (is version.dll next to the exe?)\n");
        return 1;
    }
    frame();
    frame();

    int ink = tg_menu_ink(read);
    printf("  menu shows in the readback            %s (ink %d)\n", ink > 200 ? "ok" : "WRONG", ink);
    if (ink <= 200) fails++;

    int leaked = 0;
    leaked += tg_send(WM_MOUSEMOVE, 0, MENU_X + 60, MENU_Y + 80);
    leaked += tg_send(WM_LBUTTONDOWN, MK_LBUTTON, MENU_X + 60, MENU_Y + 80);
    tg_pump();
    frame();
    for (int x = MENU_X + 60; x <= MENU_X + 260; x += 40) {
        leaked += tg_send(WM_MOUSEMOVE, MK_LBUTTON, x, MENU_Y + 80);
        tg_pump();
        frame();
    }
    leaked += tg_send(WM_MOUSEMOVE, MK_LBUTTON, TG_W - 20, MENU_Y + 80);
    leaked += tg_send(WM_LBUTTONUP, 0, TG_W - 20, MENU_Y + 80);
    printf("  slider drag never reached the game    %s (%d leaked)\n", leaked == 0 ? "ok" : "WRONG", leaked);
    if (leaked) fails++;

    int got = 0;
    got += tg_send(WM_MOUSEMOVE, 0, TG_W - 16, TG_H - 16);
    got += tg_send(WM_LBUTTONDOWN, MK_LBUTTON, TG_W - 16, TG_H - 16);
    got += tg_send(WM_LBUTTONUP, 0, TG_W - 16, TG_H - 16);
    printf("  input on empty space still reaches it  %s\n", got > 0 ? "ok" : "WRONG");
    if (got <= 0) fails++;

    printf(fails ? "  %d checks failed\n" : "  all good\n", fails);
    return fails;
}

#endif
