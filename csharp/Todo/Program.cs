using System.Collections.Generic;
using System.IO;
using System.Linq;
using GoomeSharp;

const string SaveFile = "todo.txt";
var todos = Load();
var filters = new[] { "All", "Open", "Done" };
string typed = "";
int filter = 0;
bool submit = false;

using var app = new GoomeApp("Todo", 460, 520) { Flags = AppFlags.StatusBar };
app.OnEvent += ev =>
{
    if (ev.Type == EventType.Key && ev.Key == Keys.Enter) submit = true;
};
app.Run(ui =>
{
    ui.Width(340);
    ui.Input("##new", ref typed, 96);
    ui.SameLine();
    if ((ui.Button("Add") || submit) && typed.Trim().Length > 0)
    {
        todos.Add(new Todo(typed.Trim(), false));
        typed = "";
        Save();
    }
    submit = false;
    ui.Tabs("filter", filters, ref filter);

    ui.ScrollBegin("list");
    Todo? gone = null;
    for (int i = 0; i < todos.Count; i++)
    {
        var t = todos[i];
        if ((filter == 1 && t.Done) || (filter == 2 && !t.Done)) continue;
        ui.PushId(i.ToString());
        if (ui.Checkbox(t.Text, ref t.Done)) Save();
        if (ui.ContextBegin("menu"))
        {
            if (ui.MenuItem("Delete")) gone = t;
            if (ui.MenuItem("Move to top"))
            {
                todos.RemoveAt(i);
                todos.Insert(0, t);
            }
            ui.ContextEnd();
        }
        ui.PopId();
    }
    ui.ScrollEnd();
    if (gone != null)
    {
        todos.Remove(gone);
        Save();
    }

    int left = todos.Count(t => !t.Done);
    ui.StatusBar($"{left} left, {todos.Count - left} done   right click to delete");
});

List<Todo> Load()
{
    var list = new List<Todo>();
    if (File.Exists(SaveFile))
        foreach (var line in File.ReadAllLines(SaveFile))
            if (line.Length > 2) list.Add(new Todo(line[2..], line[0] == 'x'));
    if (list.Count == 0)
    {
        list.Add(new Todo("water the plants", true));
        list.Add(new Todo("reply to the landlord", false));
        list.Add(new Todo("buy a new bike chain", false));
        list.Add(new Todo("finish the goome example", false));
    }
    return list;
}

void Save() => File.WriteAllLines(SaveFile, todos.Select(t => (t.Done ? "x " : "- ") + t.Text));

class Todo
{
    public string Text;
    public bool Done;

    public Todo(string text, bool done) => (Text, Done) = (text, done);
}
