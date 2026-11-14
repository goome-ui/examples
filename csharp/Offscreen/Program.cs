using System;
using System.IO;
using System.IO.Compression;
using GoomeSharp;

string path = args.Length > 0 ? args[0] : "offscreen.png";
using var g = Goome.OpenOffscreen(340, 300, 2);
using var ui = new Ui(g);
ui.Theme.AnimMs = 0;

double x = 10, y = 10, w = 320, h = 0, health = 0.72;
bool music = true, hard = false;
int difficulty = 1;
var modes = new[] { "Easy", "Normal", "Hard" };

for (int frame = 0; frame < 6; frame++)
{
    if (frame == 2)
    {
        g.Push(Event.Down(30, 171));
        g.Push(Event.Up(30, 171));
    }
    while (g.Poll(out var ev)) ui.Feed(ev);
    if (ui.Begin() == null) continue;
    if (ui.Window("Pause menu", ref x, ref y, ref w, ref h))
    {
        ui.Label("no window here, just pixels in memory");
        ui.Progress(health, "health");
        ui.Toggle("Music", ref music);
        ui.Checkbox("Hard mode", ref hard);
        ui.Combo("Difficulty", ref difficulty, modes);
        ui.Row(2);
        ui.Button("Resume");
        ui.Button("Quit");
    }
    ui.WindowEnd();
    ui.End();
}

var f = g.Frame();
File.WriteAllBytes(path, Png(f));
Console.WriteLine($"{path} {f.Width}x{f.Height}, hard mode {(hard ? "on" : "off")}");
return 0;

static byte[] Png(Image f)
{
    var raw = new MemoryStream();
    var px = f.Pixels;
    for (int y = 0; y < f.Height; y++)
    {
        raw.WriteByte(0);
        for (int x = 0; x < f.Width; x++)
        {
            uint s = px[y * f.Stride + x], a = s >> 24;
            for (int k = 16; k >= 0; k -= 8) raw.WriteByte((byte)(a == 0 ? 0 : (((s >> k) & 255) * 255 + a / 2) / a));
            raw.WriteByte((byte)a);
        }
    }
    var z = new MemoryStream();
    using (var zs = new ZLibStream(z, CompressionLevel.Optimal, true)) raw.WriteTo(zs);

    var png = new MemoryStream();
    png.Write(new byte[] { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A });
    var head = new byte[13];
    Put(head, 0, (uint)f.Width);
    Put(head, 4, (uint)f.Height);
    head[8] = 8;
    head[9] = 6;
    Chunk(png, "IHDR", head);
    Chunk(png, "IDAT", z.ToArray());
    Chunk(png, "IEND", Array.Empty<byte>());
    return png.ToArray();
}

static void Put(byte[] b, int at, uint v)
{
    b[at] = (byte)(v >> 24);
    b[at + 1] = (byte)(v >> 16);
    b[at + 2] = (byte)(v >> 8);
    b[at + 3] = (byte)v;
}

static void Chunk(Stream s, string type, byte[] data)
{
    var b = new byte[data.Length + 12];
    Put(b, 0, (uint)data.Length);
    for (int i = 0; i < 4; i++) b[4 + i] = (byte)type[i];
    data.CopyTo(b, 8);
    uint c = 0xFFFFFFFF;
    for (int i = 4; i < 8 + data.Length; i++)
    {
        c ^= b[i];
        for (int k = 0; k < 8; k++) c = (c & 1) != 0 ? 0xEDB88320 ^ (c >> 1) : c >> 1;
    }
    Put(b, 8 + data.Length, ~c);
    s.Write(b);
}
