<h1 align="center">🎮 native game mods 🎮</h1>

<p align="center">
<b>goome drawn inside another program's own frame, injected as a DLL.</b>
</p>

<p align="center">
These are real mod menus. The DLL loads into a game, hooks the call that shows each frame, and draws goome on top of everything the game drew, so it works in exclusive fullscreen too. Same idea as ReShade or a Dear ImGui mod menu, just with goome. There are four, one per graphics api:
</p>

| folder | hooks | for |
|---|---|---|
| [dx11](dx11) | `IDXGISwapChain::Present` | Direct3D 11 games |
| [dx12](dx12) | `Present` + `ExecuteCommandLists` | Direct3D 12 games |
| [opengl](opengl) | `wglSwapBuffers` and `SwapBuffers` | OpenGL games |
| [vulkan](vulkan) | `vkQueuePresentKHR` | Vulkan games |

## Read this first

Single player and modding only. These draw over the game and eat your mouse and keyboard while the menu is up, which is exactly what an anti-cheat looks for. Put one in an online game with anti-cheat and you will get banned. There is no injector here and nothing that hides from anti-cheat, on purpose. Don't ask for that.

## How it loads

Each mod builds to `version.dll`. Drop it next to the game's exe and Windows loads it instead of the real `version.dll`, then the mod passes every real call straight through to the system one, so the game never notices. `version.dll` is the generic way in because almost every game loads it and it only has sixteen functions to forward. A `dxgi.dll` or `winmm.dll` proxy works the same way if you'd rather use one of those, the forwarder in [common/proxy.c](common/proxy.c) just lists different names.

When it's loaded the mod spins up a thread, makes a throwaway device to read the swapchain's vtable, and hooks the present call with [MinHook](https://github.com/TsudaKageyu/minhook). The build script fetches MinHook, it isn't checked in. From then on every frame goes offscreen goome -> the matching renderer backend -> one textured quad over the game's frame.

Input goes through [common/mod.h](common/mod.h), which subclasses the game window with `goome_win32_wndproc`. While goome wants the mouse or keyboard the event is eaten and never reaches the game, so dragging a slider doesn't also turn the camera. Insert hides and shows the menu.

## Build one

```sh
bash build.sh dx11      # or dx12, opengl, vulkan
```

You need `x86_64-w64-mingw32-gcc` (or set `CC`). It drops `build/<api>/version.dll`. On Windows with MSVC the same sources compile, you just need MinHook and, for the vulkan one, the Vulkan headers on your include path.

## Does it actually work

[testgame](testgame) has a tiny game per api that spins a triangle and logs its input. CI loads each mod into it and checks that the menu shows up in a screenshot and that a scripted slider drag never once reached the game. See that folder for what runs where.
