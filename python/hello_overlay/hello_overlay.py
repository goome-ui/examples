import sys

import goome


def main():
    try:
        g = goome.open("hello overlay")
    except goome.GoomeError as e:
        print("cant open an overlay: %s" % e, file=sys.stderr)
        return 1
    with g, g.ui() as ui:
        win = goome.Window(60, 60, 300, 0)
        clicks, volume, dark = 0, 0.6, True
        while win.open:
            for ev in g.events(16):
                if ev.type == goome.QUIT:
                    win.open = False
                ui.feed(ev)
            if not win.open:
                break
            with ui.frame(0) as c:
                if c is None:
                    continue
                with ui.window("hello", win, goome.WIN_MOVE | goome.WIN_CLOSE) as shown:
                    if shown:
                        ui.label("this sits on top of everything")
                        ui.label("clicks next to it go to the app under it")
                        if ui.button("Click me"):
                            clicks += 1
                        ui.same_line()
                        ui.label("%d clicks" % clicks)
                        volume = ui.slider("Volume", volume, 0, 1, "%.2f")
                        dark = ui.toggle("Dark", dark)
                        if ui.changed:
                            ui.theme.dark() if dark else ui.theme.light()
    return 0


if __name__ == "__main__":
    sys.exit(main())
