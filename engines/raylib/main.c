#include <math.h>
#include <string.h>

#include "raylib.h"
#include "rlgl.h"

#define GOOME_IMPL_GL3_IMPLEMENTATION
#include "goome_gl3.h"

static const int special_keys[][2] = {
    {KEY_BACKSPACE, GOOME_K_BACKSPACE}, {KEY_TAB, GOOME_K_TAB}, {KEY_ENTER, GOOME_K_ENTER},
    {KEY_KP_ENTER, GOOME_K_ENTER}, {KEY_ESCAPE, GOOME_K_ESC}, {KEY_DELETE, GOOME_K_DELETE},
    {KEY_LEFT, GOOME_K_LEFT}, {KEY_RIGHT, GOOME_K_RIGHT}, {KEY_UP, GOOME_K_UP}, {KEY_DOWN, GOOME_K_DOWN},
    {KEY_HOME, GOOME_K_HOME}, {KEY_END, GOOME_K_END}, {KEY_PAGE_UP, GOOME_K_PGUP},
    {KEY_PAGE_DOWN, GOOME_K_PGDN}, {KEY_INSERT, GOOME_K_INSERT},
};

static int held_mods(void)
{
    int m = 0;
    if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) m |= GOOME_SHIFT;
    if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) m |= GOOME_CTRL;
    if (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) m |= GOOME_ALT;
    if (IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER)) m |= GOOME_SUPER;
    return m;
}

static void send(goome *g, int type, Vector2 at, int button, int dy, int key, int mods, const char *text, int len)
{
    goome_event e;
    memset(&e, 0, sizeof(e));
    e.type = type, e.x = at.x, e.y = at.y, e.button = button, e.dy = dy, e.key = key, e.mods = mods;
    if (text) memcpy(e.text, text, len < 7 ? len : 7);
    goome_push(g, &e);
}

static void feed_raylib_input(goome *g)
{
    static Vector2 last = {-1, -1};
    static float wheel;
    static const int buttons[3][2] = {{MOUSE_BUTTON_LEFT, 1}, {MOUSE_BUTTON_MIDDLE, 2}, {MOUSE_BUTTON_RIGHT, 3}};
    Vector2 at = GetMousePosition();
    int mods = held_mods();
    if (at.x != last.x || at.y != last.y) send(g, GOOME_MOTION, at, 0, 0, 0, mods, NULL, 0), last = at;
    for (int i = 0; i < 3; i++) {
        if (IsMouseButtonPressed(buttons[i][0])) send(g, GOOME_DOWN, at, buttons[i][1], 0, 0, mods, NULL, 0);
        if (IsMouseButtonReleased(buttons[i][0])) send(g, GOOME_UP, at, buttons[i][1], 0, 0, mods, NULL, 0);
    }
    wheel -= GetMouseWheelMove();
    int notches = (int)wheel;
    wheel -= notches;
    if (notches) send(g, GOOME_SCROLL, at, 0, notches, 0, mods, NULL, 0);
    for (size_t i = 0; i < sizeof(special_keys) / sizeof(special_keys[0]); i++)
        if (IsKeyPressed(special_keys[i][0]) || IsKeyPressedRepeat(special_keys[i][0]))
            send(g, GOOME_KEY, at, 0, 0, special_keys[i][1], mods, NULL, 0);
    if (mods & (GOOME_CTRL | GOOME_SUPER))
        for (int k = KEY_A; k <= KEY_Z; k++)
            if (IsKeyPressed(k)) send(g, GOOME_KEY, at, 0, 0, 'a' + (k - KEY_A), mods, NULL, 0);
    for (int cp = GetCharPressed(); cp; cp = GetCharPressed()) {
        int len, key = cp < 0x80 ? cp : 0, shift = mods & GOOME_SHIFT;
        if (key >= 'A' && key <= 'Z') key += 32, shift = GOOME_SHIFT;
        const char *utf8 = CodepointToUTF8(cp, &len);
        send(g, GOOME_KEY, at, 0, 0, key, shift, utf8, len);
    }
}

int main(void)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(960, 600, "goome in raylib");
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);

    int err;
    goome *g = goome_open(&(goome_opts){.mode = GOOME_MODE_OFFSCREEN, .width = GetScreenWidth(), .height = GetScreenHeight()}, &err);
    if (!g) {
        TraceLog(LOG_ERROR, "goome: %s", goome_strerror(err));
        CloseWindow();
        return 1;
    }
    goome_ui *u = goome_ui_new(g);
    if (goome_gl3_init(g, NULL) != GOOME_OK) {
        TraceLog(LOG_ERROR, "goome: no gl3 here");
        goome_ui_free(u);
        goome_close(g);
        CloseWindow();
        return 1;
    }

    Camera3D cam = {.up = {0, 1, 0}, .fovy = 45, .projection = CAMERA_PERSPECTIVE};
    double menu_x = 16, menu_y = 16, menu_w = 280, menu_h = 0;
    double spin_speed = 60, cube_size = 2;
    float yaw = 0.8f, pitch = 0.4f, distance = 8, spin = 0;
    int wireframe = 0, paused = 0, dragging = 0;
    uint32_t cube_color = 0xFFE0607E;
    char name[32] = "player one";

    while (!WindowShouldClose()) {
        if (IsWindowResized()) goome_gl3_resize(g, GetScreenWidth(), GetScreenHeight(), 1);

        feed_raylib_input(g);
        goome_event ev;
        while (goome_poll(g, &ev, 0) > 0) goome_ui_feed(u, &ev);
        int wants = goome_ui_wants(u);
        int ui_has_mouse = wants & GOOME_WANT_MOUSE, ui_has_keys = wants & GOOME_WANT_KEYBOARD;

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !ui_has_mouse) dragging = 1;
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) dragging = 0;
        if (dragging) {
            Vector2 d = GetMouseDelta();
            yaw -= d.x * 0.01f;
            pitch += d.y * 0.01f;
            if (pitch > 1.5f) pitch = 1.5f;
            if (pitch < -1.5f) pitch = -1.5f;
        }
        if (!ui_has_mouse) distance -= GetMouseWheelMove() * 0.5f;
        if (distance < 3) distance = 3;
        if (!ui_has_keys && IsKeyPressed(KEY_SPACE)) paused = !paused;
        if (!ui_has_keys && IsKeyPressed(KEY_ESCAPE)) break;

        if (!paused) spin += (float)spin_speed * GetFrameTime();
        cam.position = (Vector3){distance * cosf(pitch) * sinf(yaw), distance * sinf(pitch), distance * cosf(pitch) * cosf(yaw)};

        if (goome_ui_begin(u, 0)) {
            if (goome_ui_window(u, "Mod menu", &menu_x, &menu_y, &menu_w, &menu_h, GOOME_WIN_MOVE, NULL)) {
                goome_ui_label(u, "%d fps", GetFPS());
                goome_ui_slider(u, "Spin", &spin_speed, 0, 360, "%.0f deg/s");
                goome_ui_slider(u, "Size", &cube_size, 0.5, 4, "%.1f");
                goome_ui_checkbox(u, "Wireframe", &wireframe);
                goome_ui_checkbox(u, "Paused (space)", &paused);
                goome_ui_color(u, "Color", &cube_color);
                goome_ui_input(u, "Name", name, sizeof(name));
                if (goome_ui_button(u, "Reset camera")) yaw = 0.8f, pitch = 0.4f, distance = 8;
                goome_ui_separator(u);
                goome_ui_label(u, "mouse: %s", ui_has_mouse ? "menu" : "game");
                goome_ui_label(u, "keys: %s", ui_has_keys ? "menu" : "game");
            }
            goome_ui_window_end(u);
            goome_ui_end(u);
        }

        Color tint = {(cube_color >> 16) & 255, (cube_color >> 8) & 255, cube_color & 255, 255};
        float s = (float)cube_size;
        BeginDrawing();
        ClearBackground((Color){24, 26, 34, 255});
        BeginMode3D(cam);
        DrawGrid(12, 1);
        rlPushMatrix();
        rlRotatef(spin, 0, 1, 0);
        if (wireframe) DrawCubeWires((Vector3){0, s / 2, 0}, s, s, s, tint);
        else DrawCube((Vector3){0, s / 2, 0}, s, s, s, tint), DrawCubeWires((Vector3){0, s / 2, 0}, s, s, s, BLACK);
        rlPopMatrix();
        EndMode3D();
        DrawText(TextFormat("hi %s, drag to orbit, wheel to zoom", name), 16, GetScreenHeight() - 30, 20, RAYWHITE);
        rlDrawRenderBatchActive();
        goome_gl3_render(g);
        EndDrawing();
    }

    goome_gl3_shutdown(g);
    goome_ui_free(u);
    goome_close(g);
    CloseWindow();
    return 0;
}
