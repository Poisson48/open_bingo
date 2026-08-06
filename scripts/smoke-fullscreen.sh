#!/usr/bin/env bash
# Smoke : ouvre le mode plein écran Play et vérifie qu'une capture est produite.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$ROOT/build/src/openbingo"
OUT="${1:-/tmp/bingo-fs-smoke}"

[ -x "$BIN" ] || { echo "Build requis" >&2; exit 1; }
command -v xvfb-run >/dev/null || { echo "xvfb-run requis" >&2; exit 1; }

rm -rf "$OUT"
mkdir -p "$OUT"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

# Paysage phone — le mode play plein écran doit remplir l'écran.
W=800
H=360
echo "== Fullscreen smoke ${W}×${H} =="
timeout 40 xvfb-run -a -s "-screen 0 ${W}x${H}x24" env \
  XDG_DATA_HOME="$TMP/data" \
  BINGO_TEST_W="$W" BINGO_TEST_H="$H" \
  BINGO_SCREENSHOT_DIR="$OUT" \
  QT_QPA_PLATFORM=xcb \
  "$BIN" 2>&1 | tee "$TMP/log.txt" | grep -E 'Screenshot:|failed|TypeError|QML' || true

test -f "$OUT/06-play-fullscreen.png" || {
  echo "FAIL: 06-play-fullscreen.png manquant" >&2
  cat "$TMP/log.txt" >&2 || true
  exit 1
}

# Fichier non vide / non tout noir trivial (> 5 Ko typique)
SIZE=$(stat -c%s "$OUT/06-play-fullscreen.png")
if [ "$SIZE" -lt 3000 ]; then
  echo "FAIL: capture trop petite ($SIZE octets)" >&2
  exit 1
fi

echo "OK fullscreen smoke — $OUT/06-play-fullscreen.png ($SIZE octets)"
