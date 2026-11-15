#include "common.h"

#include <GL/gl.h>

static HDC dc;
static HGLRC rc;
static float spin;

static void frame(void)
{
    spin += 2;
    glViewport(0, 0, TG_W, TG_H);
    glClearColor(tg_clear[0] / 255.0f, tg_clear[1] / 255.0f, tg_clear[2] / 255.0f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.45f, -0.45f, 0);
    glRotatef(spin, 0, 0, 1);
    glScalef(0.4f, 0.4f, 1);
    glBegin(GL_TRIANGLES);
    glColor3f(1, 0.3f, 0.2f), glVertex2f(0, 1);
    glColor3f(0.2f, 1, 0.3f), glVertex2f(-1, -1);
    glColor3f(0.3f, 0.4f, 1), glVertex2f(1, -1);
    glEnd();
    SwapBuffers(dc);
}

static int readback(unsigned char *rgba)
{
    static unsigned char raw[TG_W * TG_H * 4];
    glReadBuffer(GL_FRONT);
    glReadPixels(0, 0, TG_W, TG_H, GL_RGBA, GL_UNSIGNED_BYTE, raw);
    for (int y = 0; y < TG_H; y++) memcpy(rgba + (size_t)y * TG_W * 4, raw + (size_t)(TG_H - 1 - y) * TG_W * 4, TG_W * 4);
    return 1;
}

int main(void)
{
    HWND win = tg_make_window(L"goome_tg_gl");
    if (!win) return printf("no window\n"), 1;
    dc = GetDC(win);
    PIXELFORMATDESCRIPTOR pfd;
    memset(&pfd, 0, sizeof(pfd));
    pfd.nSize = sizeof(pfd), pfd.nVersion = 1, pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA, pfd.cColorBits = 32, pfd.cAlphaBits = 8;
    int pf = ChoosePixelFormat(dc, &pfd);
    if (!pf || !SetPixelFormat(dc, pf, &pfd)) return printf("no pixel format\n"), 1;
    rc = wglCreateContext(dc);
    if (!rc || !wglMakeCurrent(dc, rc)) return printf("no gl context\n"), 1;
    int fails = tg_run("OpenGL", frame, readback);
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(rc);
    return fails != 0;
}
