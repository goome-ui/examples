import goome

app = goome.App("Hello app", 520, 340)
state = {"name": "", "greeted": False}


@app.frame
def draw(ui):
    ui.heading("Hello!")
    ui.wrapped("a normal window with a title bar, you can move it, resize it and minimize it like any other app.")
    ui.space(8)
    state["name"] = ui.input("Your name", state["name"], 64)
    if ui.button("Say hi"):
        state["greeted"] = True
    if state["greeted"]:
        ui.label("hi %s, nice to meet you" % (state["name"] or "stranger"))


app.run()
