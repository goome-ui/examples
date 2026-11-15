#define GOOME_IMPL_WIN32_IMPLEMENTATION
#define GOOME_IMPL_GL3_IMPLEMENTATION
#define GOOME_IMPL_GL2_IMPLEMENTATION
#include "goome_gl2.h"
#include "goome_gl3.h"
#include "goome_win32.h"

#include "mod.h"

typedef BOOL(WINAPI *swap_t)(HDC);
static swap_t real_wgl, real_gdi;
static int gl3, gl2;

static void overlay(HDC dc)
{
    HWND win = WindowFromDC(dc);
    RECT rc;
    if (!win || !GetClientRect(win, &rc) || !goome_mod_ensure(rc.right, rc.bottom)) return;
    goome_mod_subclass(win);
    if (!gl3 && !gl2) {
        if (goome_gl3_init(gmod.g, NULL) == GOOME_OK) gl3 = 1;
        else if (goome_gl2_init(gmod.g, NULL) == GOOME_OK) gl2 = 1;
        else return;
    }
    goome_mod_frame("OpenGL");
    if (gl3) goome_gl3_render(gmod.g);
    else goome_gl2_render(gmod.g);
}

static BOOL WINAPI my_wgl(HDC dc)
{
    overlay(dc);
    return real_wgl(dc);
}

static BOOL WINAPI my_gdi(HDC dc)
{
    overlay(dc);
    return real_gdi(dc);
}

int goome_mod_start(void)
{
    if (MH_Initialize() != MH_OK) return 0;
    MH_CreateHookApi(L"opengl32", "wglSwapBuffers", (void *)my_wgl, (void **)&real_wgl);
    MH_CreateHookApi(L"gdi32", "SwapBuffers", (void *)my_gdi, (void **)&real_gdi);
    MH_EnableHook(MH_ALL_HOOKS);
    return 1;
}

void goome_mod_stop(void)
{
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
    if (gl3) goome_gl3_shutdown(gmod.g);
    if (gl2) goome_gl2_shutdown(gmod.g);
    goome_mod_teardown();
}
