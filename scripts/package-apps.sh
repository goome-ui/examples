#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
root=$PWD
label="${1:-}"
case "$(uname -s)" in
MINGW* | MSYS* | CYGWIN*) os=windows ;;
Darwin) os=macos ;;
*) os=linux ;;
esac
if [ -z "$label" ]; then
  case "$os" in
  macos) label=macos-universal ;;
  *) label="$os-$(uname -m | sed 's/amd64/x86_64/')" ;;
  esac
fi
py="${PYTHON:-$(command -v python3 || command -v python || true)}"
if [ -z "$py" ]; then
  echo "need python to make the zips"
  exit 1
fi
flags=(-std=c99 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Ivendor)
link=()
ext=
cc="${CC:-cc}"
case "$os" in
windows) cc="${CC:-gcc}" ext=.exe link=(-static -s -mwindows) ;;
macos) flags+=(-arch arm64 -arch x86_64 -mmacosx-version-min=11.0) ;;
linux) link=(-s) ;;
esac

rm -rf build/dist
mkdir -p build/dist
for d in apps/*/; do
  name=$(basename "$d")
  bin="goome-${name#goome-}"
  out="build/dist/$bin"
  mkdir -p "$out"
  "$cc" "${flags[@]}" -o "$out/$bin$ext" "$d/main.c" vendor/goome.c ${link[@]+"${link[@]}"}
  cp "$d/README.md" "$out/README.md"
  cp LICENSE "$out/LICENSE"
  shots="$root/build/dist/check-$name"
  mkdir -p "$shots"
  (cd "$shots" && GOOME_TEST_SCRIPT="$root/$d/test.txt" GOOME_TEST_SHOTS=. "$root/$out/$bin$ext") > "$shots/log.txt" 2>&1 || {
    echo "$bin didnt run on the test backend"
    cat "$shots/log.txt"
    exit 1
  }
  if ! ls "$shots" | grep -q '\.ppm$'; then
    echo "$bin ran but drew nothing"
    exit 1
  fi
  rm -rf "$shots"
  zip="build/dist/$bin-$label.zip"
  "$py" - "$zip" "$out" "$bin" << 'EOF'
import os, sys, zipfile
zip_path, folder, top = sys.argv[1:4]
with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as z:
    for name in sorted(os.listdir(folder)):
        path = os.path.join(folder, name)
        info = zipfile.ZipInfo(top + "/" + name)
        info.compress_type = zipfile.ZIP_DEFLATED
        info.create_system = 3
        info.external_attr = (0o100755 if name.startswith(top) else 0o100644) << 16
        with open(path, "rb") as f:
            z.writestr(info, f.read())
EOF
  echo "made $zip ($(($(wc -c < "$zip") / 1024)) KB)"
done
