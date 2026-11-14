#!/usr/bin/env bash
set -uo pipefail
cd "$(dirname "$0")/.."
root=$PWD
what="${1:-all}"
case "$(uname -s)" in
MINGW* | MSYS* | CYGWIN*) ext=.exe ;;
*) ext= ;;
esac
py="${PYTHON:-$(command -v python3 || command -v python || true)}"
passed=0
failed=()

want() { [ "$what" = all ] || [ "$what" = "$1" ]; }

run() {
  local id=$1 dir=$2
  shift 2
  local out="$root/build/run/$id"
  rm -rf "$out"
  mkdir -p "$out"
  local log="$out/log.txt"
  if [ -f "$dir/test.txt" ]; then
    (cd "$out" && GOOME_TEST_SCRIPT="$root/$dir/test.txt" GOOME_TEST_SHOTS=. "$@") > "$log" 2>&1
  else
    (cd "$out" && unset GOOME_BACKEND && "$@") > "$log" 2>&1
  fi
  local rc=$?
  local pics
  pics=$(ls "$out" | grep -cE '\.(ppm|png)$')
  if [ "$rc" -eq 0 ] && [ "$pics" -gt 0 ]; then
    echo "ok     $id ($pics pictures)"
    passed=$((passed + 1))
  else
    echo "FAILED $id (exit $rc, $pics pictures)"
    sed 's/^/       /' "$log" | tail -n 20
    failed+=("$id")
  fi
}

if want c; then
  for d in c/*/; do
    name=$(basename "$d")
    if [ -x "build/c/$name$ext" ]; then run "c/$name" "c/$name" "$root/build/c/$name$ext"; fi
  done
fi
if want csharp; then
  for d in csharp/*/; do
    name=$(basename "$d")
    if [ -f "build/csharp/$name/$name.dll" ]; then run "csharp/$name" "csharp/$name" dotnet "$root/build/csharp/$name/$name.dll"; fi
  done
fi
if want unity && [ -f build/unity/preview/Preview.dll ]; then
  run unity unity/preview dotnet "$root/build/unity/preview/Preview.dll"
fi
if want python && [ -n "$py" ] && "$py" -c "import goome" 2> /dev/null; then
  for d in python/*/; do
    name=$(basename "$d")
    run "python/$name" "python/$name" "$py" "$root/python/$name/$name.py"
  done
fi

echo "$passed passed, ${#failed[@]} failed"
[ "${#failed[@]}" -eq 0 ]
