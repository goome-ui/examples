using System;
using GoomeSharp;

const int Grid = 11, Cell = 10;

Goome g;
try
{
    g = new Goome(new GoomeOptions { Title = "color picker", HideSelf = true });
}
catch (GoomeException e)
{
    Console.Error.WriteLine(e.Message);
    return 1;
}

using (g)
using (var font = Font.System())
{
    if (!g.Has(Caps.Capture)) Console.Error.WriteLine("this desktop wont let me see the screen, the loupe stays empty");
    g.Hotkey("alt+p", 1);
    string hex = "", copied = "";
    bool running = true;
    var pace = new Pacer();
    while (running)
    {
        while (g.Poll(out var ev, pace.Pace(30)))
        {
            if (ev.Type == EventType.Quit) running = false;
            if (ev.Type == EventType.Hotkey && hex.Length > 0 && g.SetClipboard(hex))
            {
                copied = hex;
                Console.WriteLine($"copied {hex}");
            }
        }
        if (!running) break;
        var c = g.Begin(0);
        if (c == null) continue;
        c.Clear();
        if (g.Pointer(out double px, out double py, out _) &&
            g.TryCapture(0, new Rect((int)px - Grid / 2, (int)py - Grid / 2, Grid, Grid), out var img) == GoomeError.Ok)
        {
            double size = Grid * Cell, x = px + 24, y = py + 24;
            c.DrawShadow(x, y, size + 16, size + 52, 12, 14, 0x70000000);
            c.DrawRoundRect(x, y, size + 16, size + 52, 12, 0xF01B1D22);
            for (int j = 0; j < Math.Min(Grid, img.Height); j++)
                for (int i = 0; i < Math.Min(Grid, img.Width); i++)
                    c.DrawRect(x + 8 + i * Cell, y + 8 + j * Cell, Cell, Cell, img[i, j]);
            uint mid = img[img.Width / 2, img.Height / 2];
            hex = $"#{mid & 0xFFFFFF:X6}";
            c.DrawFrame(x + 8 + Grid / 2 * Cell, y + 8 + Grid / 2 * Cell, Cell, Cell, 2, 0xFFFFFFFF);
            c.DrawRoundRect(x + 8, y + size + 16, 28, 28, 6, mid);
            c.DrawText(font, 15, x + 44, y + size + 14, hex, 0xFFFFFFFF);
            c.DrawText(font, 11, x + 44, y + size + 32, copied == hex ? "copied" : "alt+p to copy", 0xFF8A93A3);
        }
        g.End(c);
    }
}
return 0;
