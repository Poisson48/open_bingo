#!/usr/bin/env bash
# Teste le démarrage QML à plusieurs résolutions logiques (téléphones + paysage).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$ROOT/build/src/openbingo"
[ -x "$BIN" ] || { echo "Build requis : cmake --build build" >&2; exit 1; }

SIZES=(
  "360x640"   # petit Android
  "390x844"   # iPhone-like
  "412x915"   # Pixel
  "360x800"   # étroit
  "800x360"   # paysage play
  "1280x720"  # tablette paysage
)

FAIL=0
for size in "${SIZES[@]}"; do
  W="${size%x*}"
  H="${size#*x}"
  echo "== ${W}×${H} =="
  OUT="$(BINGO_TEST_W="$W" BINGO_TEST_H="$H" QT_QPA_PLATFORM=offscreen timeout 3 "$BIN" 2>&1 || true)"
  if echo "$OUT" | grep -qE "failed to load|is not a type|Type .* unavailable"; then
    echo "$OUT" | tail -5
    echo "ÉCHEC ${size}" >&2
    FAIL=1
  else
    echo "OK"
  fi
done

exit "$FAIL"
