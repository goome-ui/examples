# test games

A tiny "game" per graphics api, so the mods have something to load into that isn't a real game. Each one opens a window, spins a triangle (the vulkan one clears to a colour), and a camera that reacts to mouse input. Every mouse and key message it actually handles gets counted, that's its input log.

The test loads the matching mod's `version.dll` next to the exe, then:

- waits for the mod to take over the window
- reads the frame back and checks the goome menu drew on top
- scripts a press and a slider drag across the menu and checks not one message leaked into the game's input log
- clicks empty space and checks that still reaches the game

## Running it

```sh
bash build.sh          # builds the four mods and the four test games
RUNNER=native bash run.sh      # on Windows
```

On Linux it runs under wine with Xvfb and mesa:

```sh
Xvfb :9 -screen 0 1024x768x24 & export DISPLAY=:9 LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe
RUNNER=wine SKIP=vulkan bash run.sh
```

## What runs where

CI runs opengl, dx11 and dx12 for real on the Windows runner (WARP for the d3d ones) and again under wine on Linux, and they have to pass both. The vulkan one needs a working swapchain, which it gets on a real machine but not always in CI, so it's allowed to sit out when there's no backend, and it's skipped under wine where lavapipe's windowed present isn't reliable. `SKIP="dx12 vulkan"` leaves any of them out.
