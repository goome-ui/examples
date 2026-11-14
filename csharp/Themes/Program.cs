using GoomeSharp;

var presets = new[] { "Dark", "Light", "Midnight", "Paper", "Mint", "High contrast", "Rose" };
var names = new[] { "Blue", "Orange", "Green", "Purple" };
uint[] accents = { 0xFF4FB3F7, 0xFFFF7A59, 0xFF6FDC5F, 0xFFB57CFF };
int preset = (int)Preset.Midnight, own = -1;
bool dark = true, check = true, dirty = true;
double scale = 1, volume = 0.6;

GoomeApp.Run("Themes", 520, 400, ui =>
{
    if (dirty)
    {
        if (own < 0) ui.Theme.Preset((Preset)preset);
        else ui.Theme.Make(accents[own], dark, 10);
        ui.Scale(scale);
        dirty = false;
    }
    ui.Heading("Pick one");
    if (ui.Combo("Preset", ref preset, presets)) { own = -1; dirty = true; }
    ui.Heading("Or make your own");
    ui.Row(names.Length);
    for (int i = 0; i < names.Length; i++)
        if (ui.Button(names[i])) { own = i; dirty = true; }
    if (ui.Toggle("Dark", ref dark) && own >= 0) dirty = true;
    if (ui.Slider("Scale", ref scale, 0.75, 1.75, "%.2fx")) dirty = true;
    ui.Separator();
    ui.Checkbox("A checkbox", ref check);
    ui.Slider("Volume", ref volume, 0, 1, "%.2f");
    ui.Progress(volume);
    ui.Button("A button");
});
