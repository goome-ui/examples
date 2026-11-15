#!/usr/bin/env bash
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
cc="${CC:-x86_64-w64-mingw32-gcc}"
out="$here/../build"
mkdir -p "$out"

for mod in dx11 dx12 opengl vulkan; do
    CC="$cc" bash "$here/../build.sh" "$mod"
done

vh="$here/../build/deps/vulkan-headers"
warn=(-std=c11 -Wall -Wextra -Wno-unused-parameter -O2 -I"$here")

"$cc" "${warn[@]}" -o "$out/gl_game.exe" "$here/gl_game.c" -lgdi32 -lopengl32 -lversion
"$cc" "${warn[@]}" -o "$out/dx11_game.exe" "$here/dx11_game.c" -ld3d11 -ldxgi -ld3dcompiler -lversion
"$cc" "${warn[@]}" -o "$out/dx12_game.exe" "$here/dx12_game.c" -ld3d12 -ldxgi -ld3dcompiler -lversion
"$cc" "${warn[@]}" -I"$vh/include" -o "$out/vk_game.exe" "$here/vk_game.c" -lgdi32 -lversion

echo "built the test games in $out"
