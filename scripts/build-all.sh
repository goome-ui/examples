#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
root=$PWD
what="${1:-all}"
case "$(uname -s)" in
MINGW* | MSYS* | CYGWIN*) ext=.exe cc="${CC:-gcc}" ;;
*) ext= cc="${CC:-cc}" ;;
esac
flags=(-std=c99 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers ${WERROR:+-Werror} -Ivendor)
csharp_dir="${GoomeSharpDir:-$root/../csharp-goome}"
failed=0

want() { [ "$what" = all ] || [ "$what" = "$1" ]; }

if want c; then
  mkdir -p build/c
  echo "goome.h $(cat vendor/VERSION)"
  "$cc" "${flags[@]}" -c vendor/goome.c -o build/c/goome.o
  for d in c/*/; do
    name=$(basename "$d")
    if "$cc" "${flags[@]}" -o "build/c/$name$ext" "$d/main.c" build/c/goome.o; then
      echo "built c/$name"
    else
      echo "FAILED c/$name"
      failed=$((failed + 1))
    fi
  done
fi

if want csharp || want unity; then
  if ! command -v dotnet > /dev/null; then
    echo "no dotnet here, skipping c# and unity"
  elif [ ! -f "$csharp_dir/src/GoomeSharp/GoomeSharp.csproj" ]; then
    echo "cant find csharp-goome at $csharp_dir, clone it next to this repo or set GoomeSharpDir"
    failed=$((failed + 1))
  else
    export GoomeSharpDir="$csharp_dir"
    if [ -z "$(ls "$csharp_dir"/runtimes/*/native/* 2> /dev/null)" ]; then
      (cd "$csharp_dir" && ./build.sh)
    fi
    projects=()
    want csharp && projects+=(csharp/*/*.csproj)
    want unity && projects+=(unity/preview/*.csproj unity/check/*.csproj)
    for p in "${projects[@]}"; do
      dir=$(dirname "$p")
      out="build/${dir}"
      if dotnet build "$p" -c Release -o "$out" --nologo -v q --disable-build-servers; then
        echo "built $dir"
      else
        echo "FAILED $dir"
        failed=$((failed + 1))
      fi
    done
  fi
fi

if want python; then
  py="${PYTHON:-$(command -v python3 || command -v python || true)}"
  if [ -z "$py" ]; then
    echo "no python here, skipping it"
  elif ! "$py" -c "import goome" 2> /dev/null; then
    echo "python cant import goome, run: pip install git+https://github.com/goome-ui/python-goome@dev"
    failed=$((failed + 1))
  else
    for f in python/*/*.py; do
      if "$py" -m py_compile "$f"; then echo "checked $f"; else echo "FAILED $f"; failed=$((failed + 1)); fi
    done
  fi
fi

if [ "$failed" -gt 0 ]; then
  echo "$failed failed"
  exit 1
fi
echo "all built"
