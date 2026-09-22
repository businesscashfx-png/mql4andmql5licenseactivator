#!/usr/bin/env bash
# =============================================================================
# tools/build-desktop.sh — Genera BCFX_Activador_Windows.exe (x64)
#
# Requisito: un binario `zig` (>=0.13) disponible como:
#   - herramientas (auto-detectado en .venv-zig, ziglang, o PATH)
#
# Produce:  desktop/BCFX_Activador_Windows.exe
# =============================================================================
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
QJS="$ROOT/desktop/deps/quickjs"
OUT="$ROOT/desktop/BCFX_Activador_Windows.exe"

# ---------- localizar zig ----------
find_zig() {
  local z
  for z in \
    "$ROOT/tools/zig/zig" \
    "$HOME/.venv-zig/lib/python3.11/site-packages/ziglang/zig" \
    "$(python3 -c 'import ziglang, os; print(os.path.join(os.path.dirname(ziglang.__file__),"zig"))' 2>/dev/null || echo '')" \
    "$(command -v zig 2>/dev/null || echo '')"; do
    if [ -n "$z" ] && [ -x "$z" ]; then echo "$z"; return 0; fi
  done
  return 1
}

ZIG="$(find_zig || true)"
if [ -z "$ZIG" ]; then
  echo "ERROR: no se encontró zig. Instálalo con:  pip install ziglang" >&2
  exit 1
fi
echo ">>> usando zig: $ZIG ($($ZIG version 2>/dev/null || echo '?'))"

# ---------- embeber JS ----------
cd "$ROOT"
node tools/embed-js.js
[ -f "$ROOT/desktop/js_embed.h" ] || { echo "ERROR: js_embed.h no generado"; exit 1; }

# ---------- compilar el núcleo QuickJS (sin quickjs-libc: no hay POSIX en Windows) ----------
BUILD="$ROOT/desktop/.build"
mkdir -p "$BUILD"
CF="-target x86_64-windows-gnu -O2 -D_GNU_SOURCE -D__USE_MINGW_ANSI_STDIO -DCONFIG_VERSION=\"win\""

echo ">>> compilando núcleo QuickJS..."
for src in quickjs cutils libregexp libunicode dtoa; do
  if [ ! -f "$QJS/$src.c" ]; then echo "ERROR: falta $QJS/$src.c (clona quickjs en desktop/deps/quickjs)"; exit 1; fi
  $ZIG cc $CF -c -o "$BUILD/$src.obj" "$QJS/$src.c"
done

# ---------- recurso de versión/icono ----------
echo ">>> generando recurso .res..."
res="$ROOT/desktop/resource.rc"
[ -f "$res" ] || { echo "ERROR: falta $res"; exit 1; }
$ZIG rc /fo "$BUILD/app.res" "$res"

# ---------- compilar y enlazar win.c ----------
echo ">>> compilando win.c y enlazando..."
$ZIG cc $CF \
  -I"$QJS" -I"$ROOT/desktop" \
  -o "$OUT" \
  "$ROOT/desktop/win.c" \
  "$BUILD/quickjs.obj" "$BUILD/cutils.obj" "$BUILD/libregexp.obj" \
  "$BUILD/libunicode.obj" "$BUILD/dtoa.obj" \
  "$BUILD/app.res" \
  -Wl,/subsystem:windows \
  -lcomdlg32 -luser32 -lgdi32 -lcomctl32 -lshell32 -ladvapi32 -lole32 -lshlwapi \
  -lm

echo ""
echo "============================================================="
echo " EXE generado: $OUT"
ls -la "$OUT" 2>/dev/null || true
echo "============================================================="
