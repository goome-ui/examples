local ffi = require("ffi")
local utf8 = require("utf8")

ffi.cdef [[
typedef struct goome goome;
typedef struct goome_ui goome_ui;
typedef struct goome_canvas goome_canvas;
typedef struct { int x, y, w, h; } goome_rect;
typedef struct {
    const char *title;
    int backend;
    const char *state_dir;
    int no_capture, capture_fps, hide_self;
    const char *test_frames, *test_script, *test_shots;
    int mode, width, height, min_width, min_height, window_flags;
    double scale;
} goome_opts;
typedef struct {
    uint32_t *px;
    int w, h, stride;
    goome_rect area;
    int mon, has_self, has_cursor;
    goome_rect cursor;
} goome_image;
typedef struct {
    int type;
    double x, y;
    int mon, button, dy, key, mods;
    char text[8];
    int id;
    char msg[64];
} goome_event;

goome *goome_open(const goome_opts *o, int *err);
void goome_close(goome *g);
const char *goome_strerror(int err);
int goome_push(goome *g, const goome_event *ev);
int goome_poll(goome *g, goome_event *ev, int timeout_ms);
int goome_resize(goome *g, int w, int h, double scale);
int goome_hit(goome *g, double x, double y);
int goome_frame(goome *g, goome_image *out);
int goome_frame_dirty(goome *g, goome_rect *r);
int goome_frame_copy(goome *g, void *dst, int dst_stride_bytes, int format, const goome_rect *r);

goome_ui *goome_ui_new(goome *g);
void goome_ui_free(goome_ui *u);
int goome_ui_feed(goome_ui *u, const goome_event *ev);
goome_canvas *goome_ui_begin(goome_ui *u, int mon);
void goome_ui_end(goome_ui *u);
int goome_ui_wants(goome_ui *u);
int goome_ui_window(goome_ui *u, const char *title, double *x, double *y, double *w, double *h, int flags, int *open);
void goome_ui_window_end(goome_ui *u);
void goome_ui_label(goome_ui *u, const char *fmt, ...);
void goome_ui_separator(goome_ui *u);
int goome_ui_button(goome_ui *u, const char *label);
int goome_ui_checkbox(goome_ui *u, const char *label, int *v);
int goome_ui_slider(goome_ui *u, const char *label, double *v, double min, double max, const char *fmt);
int goome_ui_color(goome_ui *u, const char *label, uint32_t *argb);
int goome_ui_input(goome_ui *u, const char *label, char *buf, int cap);
]]

local MODE_OFFSCREEN = 2
local DOWN, UP, MOTION, SCROLL, KEY = 1, 2, 3, 4, 5
local FMT_RGBA_PREMUL = 1
local WANT_KEYBOARD = 2
local WIN_MOVE = 1
local SHIFT, CTRL, ALT, SUPER = 0x100, 0x200, 0x400, 0x800

local special_keys = {
    backspace = 8, tab = 9, ["return"] = 13, kpenter = 13, escape = 27, delete = 127,
    left = 0x90, right = 0x91, up = 0x92, down = 0x93, home = 0x94, ["end"] = 0x95,
    pageup = 0x96, pagedown = 0x97, insert = 0x98,
}
local love_to_goome_button = { 1, 3, 2 }

local lib_names = { Windows = "goome.dll", OSX = "libgoome.dylib" }
local goome = ffi.load(love.filesystem.getSource() .. "/" .. (lib_names[ffi.os] or "libgoome.so"))

local g, u, frame_data, frame_image
local ev = ffi.new("goome_event")
local dirty = ffi.new("goome_rect")
local frame_info = ffi.new("goome_image")
local menu = ffi.new("double[4]", 16, 16, 280, 0)
local spin_speed = ffi.new("double[1]", 2)
local paused = ffi.new("int[1]", 0)
local tint = ffi.new("uint32_t[1]", 0xFFE0607E)
local name = ffi.new("char[32]", "player one")
local mouse_held = 0
local angle, game_clicks = 0, 0
local mouse_x, mouse_y = 0, 0

local function held_mods()
    local m = 0
    if love.keyboard.isDown("lshift", "rshift") then m = m + SHIFT end
    if love.keyboard.isDown("lctrl", "rctrl") then m = m + CTRL end
    if love.keyboard.isDown("lalt", "ralt") then m = m + ALT end
    if love.keyboard.isDown("lgui", "rgui") then m = m + SUPER end
    return m
end

local function send(kind, fields)
    local e = ffi.new("goome_event")
    e.type, e.x, e.y, e.mods = kind, mouse_x, mouse_y, held_mods()
    for k, v in pairs(fields or {}) do
        if k == "text" then ffi.copy(e.text, v, math.min(#v, 7)) else e[k] = v end
    end
    goome.goome_push(g, e)
end

local function make_frame_texture()
    local w, h = love.graphics.getDimensions()
    if goome.goome_frame(g, frame_info) == 0 then w, h = frame_info.w, frame_info.h end
    frame_data = love.image.newImageData(w, h, "rgba8")
    frame_image = love.graphics.newImage(frame_data)
end

local function upload_frame()
    if goome.goome_frame_dirty(g, dirty) == 0 or goome.goome_frame(g, frame_info) ~= 0 then return end
    if frame_info.w ~= frame_data:getWidth() or frame_info.h ~= frame_data:getHeight() then
        make_frame_texture()
        dirty.x, dirty.y, dirty.w, dirty.h = 0, 0, frame_info.w, frame_info.h
    end
    local w = frame_data:getWidth()
    local base = ffi.cast("uint8_t *", frame_data:getFFIPointer()) + (dirty.y * w + dirty.x) * 4
    goome.goome_frame_copy(g, base, w * 4, FMT_RGBA_PREMUL, dirty)
    frame_image:replacePixels(frame_data)
end

local function ui_wants_keys()
    return bit.band(goome.goome_ui_wants(u), WANT_KEYBOARD) ~= 0
end

function love.load()
    love.window.setMode(960, 600, { resizable = true })
    love.window.setTitle("goome in love2d")
    love.keyboard.setKeyRepeat(true)
    local err = ffi.new("int[1]")
    g = goome.goome_open(ffi.new("goome_opts", { mode = MODE_OFFSCREEN, width = 960, height = 600 }), err)
    if g == nil then error("goome: " .. ffi.string(goome.goome_strerror(err[0]))) end
    u = goome.goome_ui_new(g)
    make_frame_texture()
end

function love.resize(w, h)
    goome.goome_resize(g, w, h, 1)
end

function love.mousemoved(x, y)
    mouse_x, mouse_y = x, y
    send(MOTION)
end

function love.mousepressed(x, y, button)
    mouse_x, mouse_y = x, y
    local b = love_to_goome_button[button]
    if not b then return end
    local eaten = mouse_held ~= 0 or goome.goome_hit(g, x, y) ~= 0
    send(DOWN, { button = b })
    if eaten then
        mouse_held = bit.bor(mouse_held, bit.lshift(1, b))
        return
    end
    if button == 1 then game_clicks = game_clicks + 1 end
end

function love.mousereleased(x, y, button)
    mouse_x, mouse_y = x, y
    local b = love_to_goome_button[button]
    if not b then return end
    mouse_held = bit.band(mouse_held, bit.bnot(bit.lshift(1, b)))
    send(UP, { button = b })
end

function love.wheelmoved(_, y)
    if y ~= 0 then send(SCROLL, { dy = -y }) end
end

function love.keypressed(key)
    local mods = held_mods()
    local code = special_keys[key]
    if not code and #key == 1 and bit.band(mods, CTRL + ALT + SUPER) ~= 0 then code = key:byte() end
    if code then send(KEY, { key = code }) end
    if ui_wants_keys() then return end
    if key == "space" then paused[0] = 1 - paused[0] end
    if key == "escape" then love.event.quit() end
end

function love.textinput(text)
    for _, cp in utf8.codes(text) do
        local ch = utf8.char(cp)
        local code, mods = cp < 0x80 and cp or 0, bit.band(held_mods(), SHIFT)
        if code >= 65 and code <= 90 then code, mods = code + 32, SHIFT end
        send(KEY, { key = code, mods = mods, text = ch })
    end
end

function love.update(dt)
    while goome.goome_poll(g, ev, 0) > 0 do goome.goome_ui_feed(u, ev) end
    if paused[0] == 0 then angle = angle + spin_speed[0] * dt end

    if goome.goome_ui_begin(u, 0) ~= nil then
        if goome.goome_ui_window(u, "Mod menu", menu, menu + 1, menu + 2, menu + 3, WIN_MOVE, nil) ~= 0 then
            goome.goome_ui_label(u, "%s", love.timer.getFPS() .. " fps")
            goome.goome_ui_slider(u, "Spin", spin_speed, 0, 8, "%.1f rad/s")
            goome.goome_ui_checkbox(u, "Paused (space)", paused)
            goome.goome_ui_color(u, "Color", tint)
            goome.goome_ui_input(u, "Name", name, 32)
            if goome.goome_ui_button(u, "Reset spin") ~= 0 then angle = 0 end
            goome.goome_ui_separator(u)
            goome.goome_ui_label(u, "%s", "clicks the game got: " .. game_clicks)
        end
        goome.goome_ui_window_end(u)
        goome.goome_ui_end(u)
    end
    upload_frame()
end

function love.draw()
    local w, h = love.graphics.getDimensions()
    local c = tonumber(tint[0])
    love.graphics.clear(0.09, 0.10, 0.13)
    love.graphics.push()
    love.graphics.translate(w / 2, h / 2)
    love.graphics.rotate(angle)
    love.graphics.setColor(bit.band(bit.rshift(c, 16), 255) / 255, bit.band(bit.rshift(c, 8), 255) / 255, bit.band(c, 255) / 255)
    love.graphics.polygon("fill", 0, -140, 121, 70, -121, 70)
    love.graphics.pop()
    love.graphics.setColor(1, 1, 1)
    love.graphics.print("hi " .. ffi.string(name) .. ", click the background, space pauses", 16, h - 30)
    love.graphics.setBlendMode("alpha", "premultiplied")
    love.graphics.draw(frame_image, 0, 0)
    love.graphics.setBlendMode("alpha")
end

function love.quit()
    goome.goome_ui_free(u)
    goome.goome_close(g)
end
