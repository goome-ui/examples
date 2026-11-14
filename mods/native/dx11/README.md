# dx11 mod

goome as a mod menu in a Direct3D 11 game. Builds to `version.dll`, drop it next to the game.

It makes a throwaway D3D11 device to read the swapchain vtable, hooks `IDXGISwapChain::Present` with MinHook, and each frame draws goome over the back buffer with the `goome_dx11` renderer backend. The window gets subclassed so the menu eats input while it wants it. Insert toggles the menu.

Build: `bash ../build.sh dx11`, then put `../build/dx11/version.dll` beside the exe.

Single player and modding only, anti-cheat will ban you. See ../README.md.
