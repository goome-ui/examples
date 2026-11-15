#!/usr/bin/env bash
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
mod="${1:-}"
case "$mod" in
dx11 | dx12 | opengl | vulkan) ;;
*)
    echo "usage: build.sh <dx11|dx12|opengl|vulkan>"
    exit 2
    ;;
esac

cc="${CC:-x86_64-w64-mingw32-gcc}"
out="$here/build/$mod"
deps="$here/build/deps"
mkdir -p "$out" "$deps"

mh="$deps/minhook"
[ -d "$mh" ] || git clone --quiet --depth 1 -b v1.3.4 https://github.com/TsudaKageyu/minhook "$mh"
shopt -s nullglob
mh_src=("$mh"/src/*.c)
if [ -d "$mh/src/HDE" ]; then
    mh_src+=("$mh"/src/HDE/*.c)
elif [ -d "$mh/src/hde" ]; then
    mh_src+=("$mh"/src/hde/*.c)
fi
shopt -u nullglob

vkinc=()
if [ "$mod" = vulkan ]; then
    vh="$deps/vulkan-headers"
    [ -d "$vh" ] || git clone --quiet --depth 1 -b v1.4.313 https://github.com/KhronosGroup/Vulkan-Headers "$vh"
    vkinc=(-I"$vh/include")
fi

libs=()
[ "$mod" = dx11 ] && libs=(-ld3d11 -ldxgi)
[ "$mod" = dx12 ] && libs=(-ld3d12 -ldxgi)

warn=(-std=c11 -Wall -Wextra -Wno-unused-parameter -Wno-cast-function-type -O2)
"$cc" "${warn[@]}" -shared -o "$out/version.dll" \
    -I"$root/vendor" -I"$root/vendor/backends" -I"$here/common" -I"$mh/include" "${vkinc[@]}" \
    "$here/$mod/mod.c" "$here/common/proxy.c" "$root/vendor/goome.c" "${mh_src[@]}" \
    -Wl,--enable-stdcall-fixup -lgdi32 -lole32 "${libs[@]}"

echo "built $out/version.dll"
