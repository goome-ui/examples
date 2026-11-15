using System;
using GoomeSharp;

Goome g;
try
{
    g = new Goome(new GoomeOptions { Title = "clock", NoCapture = true });
}
catch (GoomeException e)
{
    Console.Error.WriteLine(e.Message);
    return 1;
}

using (g)
using (var font = Font.System())
{
    g.Hotkey("alt+c", 1);
    bool running = true, shown = true;
    while (running)
    {
        while (g.Poll(out var ev, 250))
        {
            if (ev.Type == EventType.Quit) running = false;
            if (ev.Type == EventType.Hotkey) shown = !shown;
        }
        if (!running) break;
        var mons = g.Monitors;
        var c = g.Begin(0);
        if (c == null || mons.Length == 0) continue;
        c.Clear();
        if (shown)
        {
            var now = DateTime.Now;
            var area = mons[0].Area;
            double w = 250, h = 96, x = area.X + area.W - w - 24, y = area.Y + 24, cx = x + 48, cy = y + h / 2;
            c.DrawShadow(x, y, w, h, 18, 16, 0x60000000);
            c.DrawRoundRect(x, y, w, h, 18, 0xE01B1D22);
            c.DrawCircle(cx, cy, 34, 0xFF2A2E36);
            for (int i = 0; i < 12; i++)
            {
                double a = i * Math.PI / 6;
                c.DrawCircle(cx + Math.Cos(a) * 28, cy + Math.Sin(a) * 28, i % 3 == 0 ? 2 : 1.2, 0xFF8A93A3);
            }
            Hand(c, cx, cy, 17, (now.Hour % 12 + now.Minute / 60.0) / 12, 3.5, 0xFFFFFFFF);
            Hand(c, cx, cy, 25, (now.Minute + now.Second / 60.0) / 60, 2.5, 0xFFFFFFFF);
            Hand(c, cx, cy, 27, now.Second / 60.0, 1.2, 0xFF6FDC5F);
            c.DrawCircle(cx, cy, 3, 0xFF6FDC5F);
            c.DrawText(font, 34, x + 100, y + 14, now.ToString("HH:mm"), 0xFFFFFFFF);
            c.DrawText(font, 14, x + 102, y + 58, now.ToString("dddd, d MMM"), 0xFF9AA3B2);
        }
        g.End(c);
    }
}
return 0;

static void Hand(Canvas c, double cx, double cy, double len, double turns, double width, uint color)
{
    double a = turns * 2 * Math.PI - Math.PI / 2;
    c.DrawLine(cx, cy, cx + Math.Cos(a) * len, cy + Math.Sin(a) * len, width, color);
}
