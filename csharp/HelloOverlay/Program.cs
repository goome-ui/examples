using System;
using GoomeSharp;

Goome g;
try
{
    g = new Goome(new GoomeOptions { Title = "hello overlay" });
}
catch (GoomeException e)
{
    Console.Error.WriteLine(e.Message);
    return 1;
}

using (g)
using (var ui = new Ui(g))
{
    double x = 60, y = 60, w = 300, h = 0, volume = 0.6;
    bool open = true, dark = true;
    int clicks = 0;
    while (open)
    {
        while (g.Poll(out var ev, 16))
        {
            if (ev.Type == EventType.Quit) open = false;
            ui.Feed(ev);
        }
        if (!open) break;
        if (ui.Begin() == null) continue;
        if (ui.Window("hello", ref x, ref y, ref w, ref h, WindowFlags.Move | WindowFlags.Close, ref open))
        {
            ui.Label("this sits on top of everything");
            ui.Label("clicks next to it go to the app under it");
            if (ui.Button("Click me")) clicks++;
            ui.SameLine();
            ui.Label($"{clicks} clicks");
            ui.Slider("Volume", ref volume, 0, 1, "%.2f");
            if (ui.Toggle("Dark", ref dark))
            {
                if (dark) ui.Theme.Dark();
                else ui.Theme.Light();
            }
        }
        ui.WindowEnd();
        ui.End();
    }
}
return 0;
