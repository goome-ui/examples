import struct
import sys
import zlib

import goome


def png(frame):
    rows = bytearray()
    for y in range(frame.h):
        rows.append(0)
        for s in frame.row(y):
            a = s >> 24
            for k in (16, 8, 0):
                rows.append(((s >> k & 255) * 255 + a // 2) // a if a else 0)
            rows.append(a)

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    head = struct.pack(">IIBBBBB", frame.w, frame.h, 8, 6, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", head) + chunk(b"IDAT", zlib.compress(bytes(rows), 9)) + chunk(b"IEND", b"")


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "offscreen.png"
    with goome.open_offscreen(360, 300, scale=2) as g, g.ui() as ui:
        ui.theme.anim_ms = 0
        win = goome.Window(10, 10, 340, 0)
        music, hard, difficulty = True, False, 1
        for frame in range(6):
            if frame == 2:
                g.push_click(30, 171)
            for ev in g.events(0):
                ui.feed(ev)
            with ui.frame(0) as c:
                if c is None:
                    continue
                with ui.window("Pause menu", win, 0) as shown:
                    if shown:
                        ui.label("no window here, just pixels in memory")
                        ui.progress(0.72, "health")
                        music = ui.toggle("Music", music)
                        hard = ui.checkbox("Hard mode", hard)
                        difficulty = ui.combo("Difficulty", difficulty, ["Easy", "Normal", "Hard"])
                        ui.row(2)
                        ui.button("Resume")
                        ui.button("Quit")
        frame = g.last_frame()
        with open(path, "wb") as f:
            f.write(png(frame))
        print("%s %dx%d, hard mode %s" % (path, frame.w, frame.h, "on" if hard else "off"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
