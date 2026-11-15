#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
src="${1:-../c-goome}"
[ -d "$src" ] && src="$src/goome.h"
if [ ! -f "$src" ]; then
  echo "cant find $src, pass the c-goome folder or a goome.h"
  exit 1
fi
dir=$(cd "$(dirname "$src")" && pwd)
file=$(basename "$src")
if ! git -C "$dir" rev-parse HEAD > /dev/null 2>&1; then
  echo "$src isnt in a git checkout, cant tell which commit it is"
  exit 1
fi
if [ -n "$(git -C "$dir" status --porcelain -- "$file")" ]; then
  echo "$src has changes that arent committed, commit them first"
  exit 1
fi
cp "$src" vendor/goome.h
version=$(sed -n 's/^#define GOOME_VERSION "\(.*\)"/\1/p' vendor/goome.h | head -n 1)
commit=$(git -C "$dir" rev-parse HEAD)
backends="$dir/backends"
if [ -d "$backends" ]; then
  rm -rf vendor/backends
  mkdir -p vendor/backends
  cp "$backends"/*.h vendor/backends/
  printf '%s %s\nbackends/ from the same commit\n' "$version" "$commit" > vendor/VERSION
  echo "goome.h + backends $version $commit"
else
  printf '%s %s\n' "$version" "$commit" > vendor/VERSION
  echo "goome.h $(cat vendor/VERSION)"
fi
