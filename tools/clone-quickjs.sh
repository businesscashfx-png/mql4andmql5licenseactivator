#!/usr/bin/env bash
# Clona el motor QuickJS (necesario para compilar el .exe de Windows)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$ROOT/desktop/deps/quickjs"
if [ -d "$DEST/quickjs.c" ] || [ -f "$DEST/quickjs.c" ]; then
  echo "QuickJS ya está en $DEST"
else
  git clone --depth 1 https://github.com/bellard/quickjs.git "$DEST"
fi
rm -rf "$DEST/.git"
echo "QuickJS listo en $DEST"
