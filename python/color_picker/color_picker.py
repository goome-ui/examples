import sys

import goome

GRID, CELL = 11, 10


def main():
    try:
        g = goome.open("color picker", hide_self=True)
    except goome.GoomeError as e:
        print("cant open an overlay: %s" % e, file=sys.stderr)
        return 1
    font = goome.Font.system()
    with g:
        if not g.can(goome.CAP_CAPTURE):
            print("this desktop wont let me see the screen, the loupe stays empty", file=sys.stderr)
        try:
            g.hotkey("alt+p", 1)
        except goome.GoomeError:
            print("no global hotkeys here: %s" % g.error)
        hex_, copied, running = "", "", True
        pace = goome.Pacer(30)
        while running:
            for ev in g.events(pace.wait()):
                if ev.type == goome.QUIT:
                    running = False
                if ev.type == goome.HOTKEY and hex_:
                    g.clipboard = hex_
                    copied = hex_
                    print("copied " + hex_, flush=True)
            if not running:
                break
            c = g.begin(0)
            if c is None:
                continue
            c.clear()
            p = g.pointer()
            if p is not None:
                try:
                    img = g.capture(0, (int(p.x) - GRID // 2, int(p.y) - GRID // 2, GRID, GRID))
                except goome.GoomeError:
                    img = None
                if img is not None:
                    size = GRID * CELL
                    x, y = p.x + 24, p.y + 24
                    c.shadow(x, y, size + 16, size + 52, 12, 14, 0x70000000)
                    c.round_rect(x, y, size + 16, size + 52, 12, 0xF01B1D22)
                    for j in range(min(GRID, img.h)):
                        for i in range(min(GRID, img.w)):
                            c.rect(x + 8 + i * CELL, y + 8 + j * CELL, CELL, CELL, img.pixel(i, j))
                    mid = img.pixel(img.w // 2, img.h // 2)
                    hex_ = "#%06X" % (mid & 0xFFFFFF)
                    c.frame(x + 8 + GRID // 2 * CELL, y + 8 + GRID // 2 * CELL, CELL, CELL, 2, 0xFFFFFFFF)
                    c.round_rect(x + 8, y + size + 16, 28, 28, 6, mid)
                    c.font_text(font, 15, x + 44, y + size + 14, hex_, 0xFFFFFFFF)
                    c.font_text(font, 11, x + 44, y + size + 32, "copied" if copied == hex_ else "alt+p to copy", 0xFF8A93A3)
            g.end(c)
    return 0


if __name__ == "__main__":
    sys.exit(main())
