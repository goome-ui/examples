using GoomeSharp;

public class GameHud
{
    public double Health = 0.8, Volume = 0.6;
    public int Coins = 120;
    public bool Paused;

    readonly string[] tabs = { "Items", "Quests" };
    readonly string[] items = { "Sword", "Shield", "Health potion", "Map", "Rope" };
    readonly string[] quests = { "Find the lighthouse key", "Bring 5 fish to the harbor", "Fix the bridge" };
    readonly bool[] questDone = { true, false, false };
    double x = 24, y = 24, w = 280, h = 0;
    int tab, picked = 2;
    bool open = true, music = true;

    public void Draw(Ui ui)
    {
        if (open && ui.Window("Backpack", ref x, ref y, ref w, ref h, WindowFlags.Move | WindowFlags.Collapse | WindowFlags.Close, ref open))
        {
            ui.Progress(Health, $"health {Health * 100:0}%");
            ui.Label($"{Coins} coins");
            ui.Tabs("tabs", tabs, ref tab);
            if (tab == 0)
            {
                for (int i = 0; i < items.Length; i++)
                    if (ui.Selectable(items[i], i == picked)) picked = i;
                ui.Row(2);
                if (ui.Button("Use") && items[picked] == "Health potion" && Health < 1)
                {
                    Health = System.Math.Min(1, Health + 0.25);
                    ui.Toast("you feel better");
                }
                if (ui.Button("Drop")) ui.Toast($"dropped the {items[picked].ToLower()}");
            }
            else
            {
                for (int i = 0; i < quests.Length; i++) ui.Checkbox(quests[i], ref questDone[i]);
            }
        }
        ui.WindowEnd();

        double pw = 240, ph = 0, px = 0, py = 0;
        if (Paused)
        {
            var mons = ui.Goome.Monitors;
            px = (mons[0].Area.W - pw) / 2;
            py = mons[0].Area.H / 4.0;
        }
        if (Paused && ui.Window("Paused", ref px, ref py, ref pw, ref ph))
        {
            ui.Toggle("Music", ref music);
            ui.Slider("Volume", ref Volume, 0, 1, "%.2f");
            if (ui.Button("Resume")) Paused = false;
            if (ui.Button("Show backpack")) open = true;
        }
        if (Paused) ui.WindowEnd();
    }
}
