import goome

app = goome.App("Themes", 520, 400)
names = ["dark", "light", "midnight", "paper", "mint", "high_contrast", "rose"]
accents = [("Blue", 0xFF4FB3F7), ("Orange", 0xFFFF7A59), ("Green", 0xFF6FDC5F), ("Purple", 0xFFB57CFF)]
state = {"preset": 2, "own": None, "dark": True, "scale": 1.0, "dirty": True, "check": True, "volume": 0.6}


def dress(ui):
    if state["own"] is None:
        ui.theme.preset(names[state["preset"]])
    else:
        ui.theme.make(accents[state["own"]][1], state["dark"], 10)
    ui.scale(state["scale"])


@app.frame
def draw(ui):
    if state["dirty"]:
        dress(ui)
        state["dirty"] = False
    ui.heading("Pick one")
    state["preset"] = ui.combo("Preset", state["preset"], names)
    if ui.changed:
        state["own"], state["dirty"] = None, True
    ui.heading("Or make your own")
    ui.row(len(accents))
    for i, (name, color) in enumerate(accents):
        if ui.button(name):
            state["own"], state["dirty"] = i, True
    state["dark"] = ui.toggle("Dark", state["dark"])
    if ui.changed and state["own"] is not None:
        state["dirty"] = True
    state["scale"] = ui.slider("Scale", state["scale"], 0.75, 1.75, "%.2fx")
    if ui.changed:
        state["dirty"] = True
    ui.separator()
    state["check"] = ui.checkbox("A checkbox", state["check"])
    state["volume"] = ui.slider("Volume", state["volume"], 0, 1, "%.2f")
    ui.progress(state["volume"])
    ui.button("A button")


app.run()
