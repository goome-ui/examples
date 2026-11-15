using GoomeSharp;

string name = "";
bool greeted = false;

GoomeApp.Run("Hello app", 520, 340, ui =>
{
    ui.Heading("Hello!");
    ui.Wrapped("a normal window with a title bar, you can move it, resize it and minimize it like any other app.");
    ui.Space(8);
    ui.Input("Your name", ref name, 64);
    if (ui.Button("Say hi")) greeted = true;
    if (greeted) ui.Label($"hi {(name.Length > 0 ? name : "stranger")}, nice to meet you");
});
