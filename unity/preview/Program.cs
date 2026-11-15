using System;
using GoomeSharp;

const int W = 960, H = 540;
using var g = Goome.OpenOffscreen(W, H);
using var ui = new Ui(g);
ui.Theme.AnimMs = 0;
var hud = new GameHud { Paused = true };

for (int frame = 0; frame < 6; frame++)
{
    if (frame == 2)
    {
        g.Push(Event.Down(60, 268));
        g.Push(Event.Up(60, 268));
    }
    while (g.Poll(out var ev)) ui.Feed(ev);
    if (ui.Begin() == null) continue;
    hud.Draw(ui);
    ui.End();
}

var game = Canvas.Create(W, H);
game.DrawRRectGrad(0, 0, W, H * 0.62, 0, 0xFF5FA8E8, 0xFFBFE3F5);
game.DrawCircle(760, 110, 46, 0xFFFFF1B0);
double[] hills = { 0, 330, 180, 270, 360, 320, 560, 250, 760, 310, 960, 280, 960, 540, 0, 540 };
game.DrawPoly(hills, 0xFF4E9A52);
game.DrawRect(0, 400, W, 140, 0xFF3B7A40);
game.DrawRect(620, 300, 18, 110, 0xFF6B4A32);
game.DrawCircle(629, 280, 48, 0xFF2F6B35);

var f = g.Frame();
var hudPx = f.Pixels;
var outPx = new uint[W * H];
var bg = game.Pixels;
for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++)
    {
        uint s = hudPx[y * f.Stride + x], u = bg[y * W + x], a = s >> 24, v = 0xFF000000u;
        for (int k = 0; k < 24; k += 8) v |= (((s >> k) & 255) + ((u >> k) & 255) * (255 - a) / 255) << k;
        outPx[y * W + x] = v;
    }
var err = Goome.SavePpm("unity.ppm", outPx, W, H, W);
Console.WriteLine(err == GoomeError.Ok ? "wrote unity.ppm" : "couldnt write unity.ppm");
return err == GoomeError.Ok ? 0 : 1;
