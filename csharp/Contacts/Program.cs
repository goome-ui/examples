using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using GoomeSharp;

var contacts = new List<Contact>
{
    new Contact("Anna Weber", "anna.weber@mail.com", "0151 2345678", 2, ""),
    new Contact("Ben Carter", "ben.carter@mail.com", "555 0142", 0, "call after 6"),
    new Contact("Chris Moore", "cmoore@work.com", "555 0187", 1, ""),
    new Contact("Dana Lee", "dana.lee@mail.com", "555 0110", 0, ""),
    new Contact("Erik Olsen", "erik@work.com", "555 0164", 1, "new office number"),
};
var groups = new[] { "Friends", "Work", "Family" };
var tabs = new[] { "Details", "All contacts" };
int sel = 0, tab = 0, sortCol = 0;
bool sortDesc = false, askDelete = false, about = false, changed = false, favoritesOnly = false;
string search = "";

using var app = new GoomeApp("Contacts", 820, 540) { Flags = AppFlags.MenuBar | AppFlags.StatusBar };
app.OnEvent += ev =>
{
    if (ev.Type != EventType.Key || ev.Mods != Mods.Ctrl) return;
    if (ev.Key == 'n') Add();
    if (ev.Key == 's') Export();
};
app.OnClose = () => !changed || Export();

app.Run(ui =>
{
    if (ui.MenuBarBegin())
    {
        if (ui.MenuBegin("File"))
        {
            if (ui.MenuItem("New contact", "Ctrl+N")) Add();
            if (ui.MenuItem("Export to CSV", "Ctrl+S")) Export();
            ui.MenuSeparator();
            if (ui.MenuItem("Quit")) app.Quit();
            ui.MenuEnd();
        }
        if (ui.MenuBegin("Edit"))
        {
            if (ui.MenuItem("Delete contact", "Del")) askDelete = true;
            ui.MenuItem("Favorites only", null, ref favoritesOnly);
            ui.MenuEnd();
        }
        if (ui.MenuBegin("Help"))
        {
            if (ui.MenuItem("About")) about = true;
            ui.MenuEnd();
        }
        ui.MenuBarEnd();
    }

    ui.Width(240);
    ui.ScrollBegin("side");
    ui.Input("##search", ref search, 64);
    ui.Tooltip("search by name");
    for (int i = 0; i < contacts.Count; i++)
    {
        var c = contacts[i];
        if (favoritesOnly && !c.Favorite) continue;
        if (search.Length > 0 && !c.Name.Contains(search, StringComparison.OrdinalIgnoreCase)) continue;
        if (ui.Selectable($"{(c.Favorite ? "* " : "")}{c.Name}##{i}", i == sel))
        {
            sel = i;
            tab = 0;
        }
    }
    ui.Space(4);
    if (ui.Button("New contact")) Add();
    ui.ScrollEnd();
    ui.SameLine();

    ui.ScrollBegin("main");
    ui.Tabs("tabs", tabs, ref tab);
    ui.Space(6);
    if (tab == 0 && sel < contacts.Count)
    {
        var c = contacts[sel];
        ui.Heading(c.Name.Length > 0 ? c.Name : "New contact");
        ui.Row(2);
        changed |= ui.Input("Name", ref c.Name, 64);
        changed |= ui.Combo("Group", ref c.Group, groups);
        ui.Row(2);
        changed |= ui.Input("Email", ref c.Email, 96);
        changed |= ui.Input("Phone", ref c.Phone, 32);
        changed |= ui.Toggle("Favorite", ref c.Favorite);
        ui.Label("Notes");
        changed |= ui.TextEdit("notes", ref c.Notes, 90, EditFlags.Wrap);
        ui.Space(6);
        ui.Row(2);
        if (ui.Button("Delete")) askDelete = true;
        if (ui.Button("Export all")) Export();
    }
    else
    {
        if (ui.TableBegin("all", 4, 0, TableFlags.Sortable | TableFlags.Stripes | TableFlags.Resizable)) ShowTable(ui);
        ui.TableEnd();
    }
    ui.ScrollEnd();

    if (askDelete && ui.ModalBegin("Delete this contact?", ref askDelete))
    {
        ui.Label($"{contacts[sel].Name} goes away for good");
        ui.Row(2);
        if (ui.Button("Delete") && contacts.Count > 1)
        {
            contacts.RemoveAt(sel);
            sel = Math.Min(sel, contacts.Count - 1);
            changed = true;
            askDelete = false;
            ui.Toast("Deleted", 2000);
        }
        if (ui.Button("Keep")) askDelete = false;
        ui.ModalEnd();
    }
    if (about && ui.ModalBegin("About Contacts", ref about))
    {
        ui.Label("a small address book, like the WinForms ones");
        ui.Label($"made with goome {Goome.Version}");
        if (ui.Button("Close")) about = false;
        ui.ModalEnd();
    }
    int favs = contacts.Count(c => c.Favorite);
    ui.StatusBar($"{contacts.Count} contacts   {favs} favorite{(favs == 1 ? "" : "s")}   {(changed ? "not exported" : "exported")}");
});

void ShowTable(Ui ui)
{
    ui.TableColumn("Name", 150);
    ui.TableColumn("Email");
    ui.TableColumn("Phone", 140);
    ui.TableColumn("Group", 80);
    if (ui.TableSort(ref sortCol, ref sortDesc))
    {
        var picked = contacts[sel];
        Func<Contact, string> key = sortCol switch { 1 => c => c.Email, 2 => c => c.Phone, 3 => c => groups[c.Group], _ => c => c.Name };
        contacts = (sortDesc ? contacts.OrderByDescending(key) : contacts.OrderBy(key)).ToList();
        sel = contacts.IndexOf(picked);
    }
    foreach (var c in contacts)
    {
        ui.TableRow();
        ui.Label(c.Name);
        ui.TableNext();
        ui.Label(c.Email);
        ui.TableNext();
        ui.Label(c.Phone);
        ui.TableNext();
        ui.Label(groups[c.Group]);
    }
}

void Add()
{
    contacts.Add(new Contact("", "", "", 0, ""));
    sel = contacts.Count - 1;
    tab = 0;
    changed = true;
}

bool Export()
{
    try
    {
        File.WriteAllLines("contacts.csv", contacts.Select(c => $"\"{c.Name}\",\"{c.Email}\",\"{c.Phone}\",{groups[c.Group]}").Prepend("name,email,phone,group"));
        changed = false;
        app.Ui.Toast("Saved contacts.csv", 2000);
        return true;
    }
    catch (IOException e)
    {
        app.Ui.Toast($"couldnt save: {e.Message}", 4000);
        return false;
    }
}

class Contact
{
    public string Name, Email, Phone, Notes;
    public int Group;
    public bool Favorite;

    public Contact(string name, string email, string phone, int group, string notes)
    {
        (Name, Email, Phone, Group, Notes) = (name, email, phone, group, notes);
        Favorite = group == 2;
    }
}
