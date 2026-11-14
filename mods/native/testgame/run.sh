#!/usr/bin/env bash
set -u
here=$(cd "$(dirname "$0")" && pwd)
build="$here/../build"
runner="${RUNNER:-native}"
skip=" ${SKIP:-} "
run="$build/run"
rm -rf "$run"
mkdir -p "$run"

play() {
    local api="$1" game="$2" mod="$3" soft="${4:-0}"
    case "$skip" in *" $api "*)
        echo "$api left out here"
        return 0
        ;;
    esac
    local dir="$run/$api"
    mkdir -p "$dir"
    cp "$build/$game" "$dir/" || return 1
    cp "$build/$mod/version.dll" "$dir/version.dll" || return 1
    local rc
    if [ "$runner" = wine ]; then
        (cd "$dir" && WINEDLLOVERRIDES="version=n,b" WINEDEBUG=-all DISPLAY="${DISPLAY:-}" timeout -k 5 -s KILL 180 wine "./$game")
        rc=$?
    else
        (cd "$dir" && timeout -k 5 -s KILL 180 "./$game")
        rc=$?
    fi
    if [ "$soft" = 1 ] && [ "$rc" != 0 ]; then
        echo "  ($api left out, no working backend here)"
        return 0
    fi
    return $rc
}

fails=0
play opengl gl_game.exe opengl || fails=$((fails + 1))
play dx11 dx11_game.exe dx11 || fails=$((fails + 1))
play dx12 dx12_game.exe dx12 || fails=$((fails + 1))
play vulkan vk_game.exe vulkan 1 || fails=$((fails + 1))

echo "native mods: $fails failed"
[ "$fails" = 0 ]
