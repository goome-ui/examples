import os

import goome

PATH = "todo.txt"


def load():
    todos = []
    if os.path.exists(PATH):
        with open(PATH, encoding="utf-8") as f:
            for line in f.read().splitlines():
                if len(line) > 2:
                    todos.append([line[2:], line[0] == "x"])
    return todos or [["water the plants", True], ["reply to the landlord", False], ["buy a new bike chain", False], ["finish the goome example", False]]


def save():
    with open(PATH, "w", encoding="utf-8") as f:
        f.writelines(("x " if done else "- ") + text + "\n" for text, done in todos)


todos = load()
state = {"typed": "", "filter": 0, "submit": False}
app = goome.App("Todo", 460, 520, flags=goome.APP_STATUSBAR)


@app.event
def keys(ev):
    if ev.type == goome.KEY and ev.key == goome.K_ENTER:
        state["submit"] = True


@app.frame
def draw(ui):
    ui.width(340)
    state["typed"] = ui.input("##new", state["typed"], 96)
    ui.same_line()
    if (ui.button("Add") or state["submit"]) and state["typed"].strip():
        todos.append([state["typed"].strip(), False])
        state["typed"] = ""
        save()
    state["submit"] = False
    state["filter"] = ui.tabs("filter", ["All", "Open", "Done"], state["filter"])

    gone = None
    ui.scroll_begin("list")
    for i, todo in enumerate(todos):
        if (state["filter"] == 1 and todo[1]) or (state["filter"] == 2 and not todo[1]):
            continue
        with ui.id(str(i)):
            todo[1] = ui.checkbox(todo[0], todo[1])
            if ui.changed:
                save()
            with ui.context("menu") as shown:
                if shown and ui.menu_item("Delete"):
                    gone = i
    ui.scroll_end()
    if gone is not None:
        del todos[gone]
        save()

    left = sum(1 for t in todos if not t[1])
    ui.statusbar("%d left, %d done   right click to delete" % (left, len(todos) - left))


app.run()
