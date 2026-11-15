using GoomeSharp;
using GoomeUI;
using UnityEngine;

public class HudView : MonoBehaviour
{
    public GoomeView view;
    readonly GameHud hud = new GameHud();
    float coinTimer;
    bool pauseHeld;

    void OnEnable()
    {
        if (view == null) view = GetComponent<GoomeView>();
        view.Draw += hud.Draw;
    }

    void OnDisable()
    {
        if (view != null) view.Draw -= hud.Draw;
    }

    void Update()
    {
        coinTimer += Time.deltaTime;
        if (coinTimer > 1)
        {
            coinTimer = 0;
            hud.Coins++;
        }
        bool pause = Input.GetKey(KeyCode.P);
        if (pause && !pauseHeld) hud.Paused = !hud.Paused;
        pauseHeld = pause;
        if (Input.GetMouseButtonDown(0) && !view.PointerOverUi) hud.Health = Mathf.Max(0, (float)hud.Health - 0.05f);
    }
}
