# dx12 mod

goome as a mod menu in a Direct3D 12 game. Builds to `version.dll`, drop it next to the game.

D3D12 doesn't hand you the command queue, so it hooks `ExecuteCommandLists` to catch the one the game uses, plus `Present` to draw. Each frame it records its own command list that transitions the back buffer, draws goome with the `goome_dx12` backend over it, and submits on the game's queue. A fence keeps the per-frame allocators straight. Insert toggles the menu.

Build: `bash ../build.sh dx12`, then put `../build/dx12/version.dll` beside the exe.

Single player and modding only, anti-cheat will ban you. See ../README.md.
