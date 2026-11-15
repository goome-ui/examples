import math
import sys
import time

import goome


def hand(c, cx, cy, length, turns, width, color):
    a = turns * 2 * math.pi - math.pi / 2
    c.line(cx, cy, cx + math.cos(a) * length, cy + math.sin(a) * length, width, color)


def main():
    try:
        g = goome.open("clock", no_capture=True)
    except goome.GoomeError as e:
        print("cant open an overlay: %s" % e, file=sys.stderr)
        return 1
    font = goome.Font.system()
    with g:
        try:
            g.hotkey("alt+c", 1)
        except goome.GoomeError:
            print("no global hotkeys here: %s" % g.error)
        running, shown = True, True
        while running:
            for ev in g.events(250):
                if ev.type == goome.QUIT:
                    running = False
                if ev.type == goome.HOTKEY:
                    shown = not shown
            if not running:
                break
            area = g.monitors()[0].area
            c = g.begin(0)
            if c is None:
                continue
            c.clear()
            if shown:
                now = time.localtime()
                w, h = 250, 96
                x, y = area.x + area.w - w - 24, area.y + 24
                cx, cy = x + 48, y + h / 2
                c.shadow(x, y, w, h, 18, 16, 0x60000000)
                c.round_rect(x, y, w, h, 18, 0xE01B1D22)
                c.circle(cx, cy, 34, 0xFF2A2E36)
                for i in range(12):
                    a = i * math.pi / 6
                    c.circle(cx + math.cos(a) * 28, cy + math.sin(a) * 28, 1.2 if i % 3 else 2, 0xFF8A93A3)
                hand(c, cx, cy, 17, (now.tm_hour % 12 + now.tm_min / 60) / 12, 3.5, 0xFFFFFFFF)
                hand(c, cx, cy, 25, (now.tm_min + now.tm_sec / 60) / 60, 2.5, 0xFFFFFFFF)
                hand(c, cx, cy, 27, now.tm_sec / 60, 1.2, 0xFF6FDC5F)
                c.circle(cx, cy, 3, 0xFF6FDC5F)
                c.font_text(font, 34, x + 100, y + 14, time.strftime("%H:%M", now), 0xFFFFFFFF)
                c.font_text(font, 14, x + 102, y + 58, time.strftime("%A, %d %b", now), 0xFF9AA3B2)
            g.end(c)
    return 0


if __name__ == "__main__":
    sys.exit(main())
