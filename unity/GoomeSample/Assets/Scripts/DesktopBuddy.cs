using GoomeSharp;
using GoomeUI;
using UnityEngine;

public class DesktopBuddy : MonoBehaviour
{
    public GoomeOverlay overlay;
    double x = 60, y = 60, w = 260, h = 0;
    bool open = true;
    int pets;

    void OnEnable()
    {
        if (overlay == null) overlay = GetComponent<GoomeOverlay>();
        overlay.Draw += DrawUi;
    }

    void OnDisable()
    {
        if (overlay != null) overlay.Draw -= DrawUi;
    }

    void DrawUi(Ui ui)
    {
        if (ui.Window("Desktop buddy", ref x, ref y, ref w, ref h, WindowFlags.Move | WindowFlags.Close, ref open))
        {
            ui.Label("this one floats over your whole desktop");
            if (ui.Button("Pet")) pets++;
            ui.SameLine();
            ui.Label($"{pets} pets");
        }
        ui.WindowEnd();
        if (!open) Application.Quit();
    }
}
