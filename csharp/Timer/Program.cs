using System;
using GoomeSharp;

var kinds = new[] { "Focus", "Break", "Long break" };
int[] minutes = { 25, 5, 15 };
int kind = 0, done = 0;
bool running = false;
long length = minutes[0] * 60000L, left = length, last = Goome.NowMs;
const int RingW = 292, RingH = 180;
var ring = Canvas.Create(RingW * 2, RingH * 2, 0, 2);
using var picture = Bitmap.Wrap(new uint[RingW * 2 * RingH * 2], RingW * 2, RingH * 2);

using var app = new GoomeApp(new GoomeOptions { Title = "Timer", Mode = Mode.Window, Width = 320, Height = 380, WindowStyle = WindowStyle.OnTop | WindowStyle.Center });
app.RedrawEveryMs = 200;
app.Run(ui =>
{
    long now = Goome.NowMs;
    if (running) left -= now - last;
    last = now;
    if (running && left <= 0)
    {
        left = 0;
        running = false;
        done++;
        ui.Toast("Time is up", 3000);
    }

    int was = kind;
    ui.Tabs("kind", kinds, ref kind);
    if (kind != was)
    {
        running = false;
        length = left = minutes[kind] * 60000L;
    }

    long secs = (left + 999) / 1000;
    DrawRing(ui, $"{secs / 60:00}:{secs % 60:00}", (double)left / length, kind == 0 ? ui.Theme.Accent : 0xFF5FB8F0);
    ui.Space(6);
    ui.Image(picture, RingW, RingH);
    ui.Space(6);
    ui.Row(2);
    if (ui.Button(running ? "Pause" : "Start")) running = !running;
    if (ui.Button("Reset"))
    {
        running = false;
        left = length;
    }
    ui.Label($"{done} done today");
});

void DrawRing(Ui ui, string text, double frac, uint color)
{
    ring.Pixels.Clear();
    double cx = RingW / 2.0, cy = RingH / 2.0;
    ring.DrawRing(cx, cy, cy - 12, 14, 0x30FFFFFF);
    if (frac > 0.001) ring.DrawArc(cx, cy, cy - 12, 14, -Math.PI / 2, -Math.PI / 2 + 2 * Math.PI * frac, color);
    var font = ui.Theme.Font;
    ring.DrawText(font, 40, cx - font.Measure(40, text) / 2, cy - font.LineHeight(40) / 2, text, 0xFFFFFFFF);
    ring.Pixels.CopyTo(picture.Pixels);
}
