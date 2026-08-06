#!/usr/bin/env bash
# Génère les captures pour README et page GitHub (Xvfb + mode BINGO_SCREENSHOT_DIR).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$ROOT/build/src/openbingo"
OUT="${1:-$ROOT/docs/assets/screenshots}"
PHONE_W=390
PHONE_H=844
DESKTOP_W=1200
DESKTOP_H=720

[ -x "$BIN" ] || { echo "Build requis : cmake --build build" >&2; exit 1; }
command -v xvfb-run >/dev/null || { echo "xvfb-run requis" >&2; exit 1; }

mkdir -p "$OUT/phone" "$OUT/desktop"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

run_shots() {
  local w="$1" h="$2" subdir="$3"
  echo "== Captures ${w}×${h} → $OUT/$subdir =="
  timeout 35 xvfb-run -a -s "-screen 0 ${w}x${h}x24" env \
    XDG_DATA_HOME="$TMP/data-$subdir" \
    BINGO_TEST_W="$w" BINGO_TEST_H="$h" \
    BINGO_SCREENSHOT_DIR="$OUT/$subdir" \
    QT_QPA_PLATFORM=xcb \
    "$BIN" 2>&1 | grep -E 'Screenshot:|failed|error|TypeError' || true
}

run_shots "$PHONE_W" "$PHONE_H" phone
run_shots "$DESKTOP_W" "$DESKTOP_H" desktop

echo
echo "OK — $(find "$OUT" -name '*.png' | wc -l) captures dans $OUT"
