import sys

import goome

COLORS = [0xFF6FDC5F, 0xFFFF4F6B, 0xFF4FC3FF, 0xFFFFFFFF]
STYLES = ["cross", "dot", "circle"]


def draw(c, style, cx, cy, s, color):
    edge = 0xA0000000
    if style == "cross":
        for width, k in ((4, edge), (2, color)):
            c.line(cx - s - 4, cy, cx - 4, cy, width, k)
            c.line(cx + 4, cy, cx + s + 4, cy, width, k)
            c.line(cx, cy - s - 4, cx, cy - 4, width, k)
            c.line(cx, cy + 4, cx, cy + s + 4, width, k)
    elif style == "dot":
        c.circle(cx, cy, s / 4 + 1.5, edge)
        c.circle(cx, cy, s / 4, color)
    else:
        c.ring(cx, cy, s, 4, edge)
        c.ring(cx, cy, s, 2, color)
        c.circle(cx, cy, 1.5, color)


def main():
    try:
        g = goome.open("crosshair", no_capture=True)
    except goome.GoomeError as e:
        print("cant open an overlay: %s" % e, file=sys.stderr)
        return 1
    with g:
        keys = {"alt+x": 1, "alt+up": 2, "alt+down": 3, "alt+h": 4}
        for combo, id in keys.items():
            try:
                g.hotkey(combo, id)
            except goome.GoomeError:
                print("cant grab %s: %s" % (combo, g.error))
        style, size, color, shown, running = 0, 12, 0, True, True
        while running:
            for ev in g.events(1000):
                if ev.type == goome.QUIT:
                    running = False
                if ev.type == goome.HOTKEY:
                    if ev.id == 1:
                        style, color = (style + 1) % len(STYLES), (color + 1) % len(COLORS)
                    elif ev.id == 2:
                        size = min(40, size + 2)
                    elif ev.id == 3:
                        size = max(4, size - 2)
                    elif ev.id == 4:
                        shown = not shown
            if not running:
                break
            area = g.monitors()[0].area
            with g.frame(0) as c:
                if c is None:
                    continue
                c.clear()
                if shown:
                    draw(c, STYLES[style], area.x + area.w / 2, area.y + area.h / 2, size, COLORS[color])
    return 0


if __name__ == "__main__":
    sys.exit(main())
