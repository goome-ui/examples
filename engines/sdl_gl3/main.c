#include <math.h>
#include <stdio.h>
#include <string.h>

#include <SDL.h>
#include <SDL_opengl.h>

#define GOOME_IMPL_SDL2_IMPLEMENTATION
#include "goome_sdl2.h"
#define GOOME_IMPL_GL3_IMPLEMENTATION
#include "goome_gl3.h"

static struct {
    void(APIENTRY *Viewport)(GLint, GLint, GLsizei, GLsizei);
    void(APIENTRY *ClearColor)(GLfloat, GLfloat, GLfloat, GLfloat);
    void(APIENTRY *Clear)(GLbitfield);
    void(APIENTRY *DrawArrays)(GLenum, GLint, GLsizei);
    GLuint(APIENTRY *CreateShader)(GLenum);
    void(APIENTRY *ShaderSource)(GLuint, GLsizei, const GLchar *const *, const GLint *);
    void(APIENTRY *CompileShader)(GLuint);
    GLuint(APIENTRY *CreateProgram)(void);
    void(APIENTRY *AttachShader)(GLuint, GLuint);
    void(APIENTRY *LinkProgram)(GLuint);
    void(APIENTRY *UseProgram)(GLuint);
    GLint(APIENTRY *GetUniformLocation)(GLuint, const GLchar *);
    void(APIENTRY *Uniform1f)(GLint, GLfloat);
    void(APIENTRY *Uniform3f)(GLint, GLfloat, GLfloat, GLfloat);
    void(APIENTRY *GenVertexArrays)(GLsizei, GLuint *);
    void(APIENTRY *BindVertexArray)(GLuint);
} gl;

static const char *gl_names[] = {
    "glViewport", "glClearColor", "glClear", "glDrawArrays", "glCreateShader", "glShaderSource",
    "glCompileShader", "glCreateProgram", "glAttachShader", "glLinkProgram", "glUseProgram",
    "glGetUniformLocation", "glUniform1f", "glUniform3f", "glGenVertexArrays", "glBindVertexArray",
};

static const char *vertex_src =
    "#version 330 core\n"
    "uniform float angle;\n"
    "uniform float aspect;\n"
    "out vec3 shade;\n"
    "void main() {\n"
    "    float a = angle + float(gl_VertexID) * 2.0944;\n"
    "    gl_Position = vec4(0.7 * sin(a) / aspect, 0.7 * cos(a), 0.0, 1.0);\n"
    "    shade = vec3(gl_VertexID == 0, gl_VertexID == 1, gl_VertexID == 2);\n"
    "}\n";

static const char *fragment_src =
    "#version 330 core\n"
    "uniform vec3 tint;\n"
    "in vec3 shade;\n"
    "out vec4 color;\n"
    "void main() { color = vec4(mix(shade, tint, 0.5), 1.0); }\n";

static goome_gl_proc load_gl(const char *name)
{
    void *p = SDL_GL_GetProcAddress(name);
    goome_gl_proc f;
    memcpy(&f, &p, sizeof(f));
    return f;
}

static int load_triangle_gl(void)
{
    goome_gl_proc *slot = (goome_gl_proc *)(void *)&gl;
    for (size_t i = 0; i < sizeof(gl_names) / sizeof(gl_names[0]); i++)
        if (!(slot[i] = load_gl(gl_names[i]))) return 0;
    return 1;
}

static GLuint build_program(void)
{
    GLuint vs = gl.CreateShader(GL_VERTEX_SHADER), fs = gl.CreateShader(GL_FRAGMENT_SHADER), prog = gl.CreateProgram();
    gl.ShaderSource(vs, 1, &vertex_src, NULL);
    gl.ShaderSource(fs, 1, &fragment_src, NULL);
    gl.CompileShader(vs);
    gl.CompileShader(fs);
    gl.AttachShader(prog, vs);
    gl.AttachShader(prog, fs);
    gl.LinkProgram(prog);
    return prog;
}

int main(int argc, char **argv)
{
    (void)argc, (void)argv;
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return fprintf(stderr, "sdl: %s\n", SDL_GetError()), 1;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#ifdef __APPLE__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#endif
    SDL_Window *win = SDL_CreateWindow("goome in sdl2 + gl3", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 960, 600,
                                       SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_GLContext ctx = win ? SDL_GL_CreateContext(win) : NULL;
    if (!ctx || !load_triangle_gl()) return fprintf(stderr, "no gl 3.3: %s\n", SDL_GetError()), 1;
    SDL_GL_SetSwapInterval(1);

    int err;
    goome *g = goome_open(&(goome_opts){.mode = GOOME_MODE_OFFSCREEN, .width = 960, .height = 600}, &err);
    if (!g) return fprintf(stderr, "goome: %s\n", goome_strerror(err)), 1;
    goome_ui *u = goome_ui_new(g);
    goome_sdl2_init(g, win);
    goome_sdl2_set_ui(g, u);
    if (goome_gl3_init(g, load_gl) != GOOME_OK) return fprintf(stderr, "goome: gl3 renderer wont start\n"), 1;

    GLuint prog = build_program(), vao;
    gl.GenVertexArrays(1, &vao);
    GLint angle_at = gl.GetUniformLocation(prog, "angle"), aspect_at = gl.GetUniformLocation(prog, "aspect");
    GLint tint_at = gl.GetUniformLocation(prog, "tint");

    double menu_x = 16, menu_y = 16, menu_w = 280, menu_h = 0;
    double spin_speed = 1.5, angle = 0;
    uint32_t tint = 0xFFFFFFFF;
    int running = 1, paused = 0, direction = 1, game_clicks = 0, eaten = 0;
    char motd[48] = "type here, space wont pause";
    Uint64 last = SDL_GetPerformanceCounter();

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
            if (goome_sdl2_event(g, &e)) {
                eaten++;
                continue;
            }
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) game_clicks++, direction = -direction;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_SPACE) paused = !paused;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) running = 0;
        }

        goome_sdl2_frame(g);
        goome_event ev;
        while (goome_poll(g, &ev, 0) > 0) goome_ui_feed(u, &ev);

        Uint64 now = SDL_GetPerformanceCounter();
        double dt = (double)(now - last) / SDL_GetPerformanceFrequency();
        last = now;
        if (!paused) angle += direction * spin_speed * dt;

        if (goome_ui_begin(u, 0)) {
            if (goome_ui_window(u, "Mod menu", &menu_x, &menu_y, &menu_w, &menu_h, GOOME_WIN_MOVE, NULL)) {
                goome_ui_label(u, "%.0f fps", dt > 0 ? 1 / dt : 0);
                goome_ui_slider(u, "Spin", &spin_speed, 0, 6, "%.1f rad/s");
                goome_ui_checkbox(u, "Paused (space)", &paused);
                goome_ui_color(u, "Tint", &tint);
                goome_ui_input(u, "Message", motd, sizeof(motd));
                if (goome_ui_button(u, "Flip direction")) direction = -direction;
                goome_ui_separator(u);
                goome_ui_label(u, "clicks the game got: %d", game_clicks);
                goome_ui_label(u, "events goome ate: %d", eaten);
            }
            goome_ui_window_end(u);
            goome_ui_end(u);
        }

        int fw, fh;
        SDL_GL_GetDrawableSize(win, &fw, &fh);
        gl.Viewport(0, 0, fw, fh);
        gl.ClearColor(0.09f, 0.10f, 0.13f, 1);
        gl.Clear(GL_COLOR_BUFFER_BIT);
        gl.UseProgram(prog);
        gl.Uniform1f(angle_at, (float)angle);
        gl.Uniform1f(aspect_at, fh > 0 ? (float)fw / fh : 1);
        gl.Uniform3f(tint_at, ((tint >> 16) & 255) / 255.0f, ((tint >> 8) & 255) / 255.0f, (tint & 255) / 255.0f);
        gl.BindVertexArray(vao);
        gl.DrawArrays(GL_TRIANGLES, 0, 3);
        goome_gl3_render(g);
        SDL_GL_SwapWindow(win);
    }

    goome_gl3_shutdown(g);
    goome_sdl2_shutdown(g);
    goome_ui_free(u);
    goome_close(g);
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
