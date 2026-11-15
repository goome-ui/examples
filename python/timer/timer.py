import math

import goome

KINDS = ["Focus", "Break", "Long break"]
MINUTES = [25, 5, 15]
RING_W, RING_H = 292, 180

state = {"kind": 0, "running": False, "done": 0, "length": MINUTES[0] * 60000, "last": goome.now_ms()}
state["left"] = state["length"]
app = goome.App("Timer", 320, 380, window_flags=goome.WINDOW_ON_TOP | goome.WINDOW_CENTER)
ring = goome.Canvas.new(RING_W * 2, RING_H * 2, scale=2)
picture = goome.Bitmap.wrap(ring.pixels, RING_W * 2, RING_H * 2)


def draw_ring(ui, text, frac, color):
    ring.clear()
    cx, cy = RING_W / 2, RING_H / 2
    ring.ring(cx, cy, cy - 12, 14, 0x30FFFFFF)
    if frac > 0.001:
        ring.arc(cx, cy, cy - 12, 14, -math.pi / 2, -math.pi / 2 + 2 * math.pi * frac, color)
    font = ui.theme.font
    ring.font_text(font, 40, cx - goome.measure(text, 40, font)[0] / 2, cy - goome.line_height(40, font) / 2, text, 0xFFFFFFFF)


@app.frame
def draw(ui):
    now = goome.now_ms()
    if state["running"]:
        state["left"] -= now - state["last"]
    state["last"] = now
    if state["running"] and state["left"] <= 0:
        state.update(left=0, running=False, done=state["done"] + 1)
        ui.toast("Time is up", 3000)

    kind = ui.tabs("kind", KINDS, state["kind"])
    if kind != state["kind"]:
        state.update(kind=kind, running=False, length=MINUTES[kind] * 60000, left=MINUTES[kind] * 60000)

    secs = (state["left"] + 999) // 1000
    color = ui.theme.accent if state["kind"] == 0 else 0xFF5FB8F0
    draw_ring(ui, "%02d:%02d" % (secs // 60, secs % 60), state["left"] / state["length"], color)
    ui.space(6)
    ui.image(picture, RING_W, RING_H)
    ui.space(6)
    ui.row(2)
    if ui.button("Pause" if state["running"] else "Start"):
        state["running"] = not state["running"]
    if ui.button("Reset"):
        state.update(running=False, left=state["length"])
    ui.label("%d done today" % state["done"])
    app.redraw()


app.run()
