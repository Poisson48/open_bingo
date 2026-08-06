#!/usr/bin/env bash
# Lance l'app avec un vrai serveur X (Xvfb) — pas offscreen minimal.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$ROOT/build/src/openbingo"
[ -x "$BIN" ] || { echo "Build requis" >&2; exit 1; }

command -v xvfb-run >/dev/null || { echo "xvfb-run requis" >&2; exit 1; }

LOG="$(mktemp -t openbingo-live.XXXXXX.log)"
cleanup() { rm -f "$LOG"; }
trap cleanup EXIT

echo "== Lancement live (Xvfb + xcb, 390×844, 12s) =="

xvfb-run -a -s "-screen 0 390x844x24" env \
  BINGO_TEST_W=390 BINGO_TEST_H=844 \
  QT_QPA_PLATFORM=xcb \
  "$BIN" >"$LOG" 2>&1 &
PID=$!

ALIVE=0
for i in $(seq 1 12); do
  if ! kill -0 "$PID" 2>/dev/null; then
    echo "CRASH après ${i}s"
    cat "$LOG"
    exit 1
  fi
  sleep 1
  ALIVE=$i
done

kill "$PID" 2>/dev/null || true
wait "$PID" 2>/dev/null || true

if grep -qE "failed to load|is not a type|Segmentation|Aborted" "$LOG"; then
  echo "Erreurs dans le log :"
  cat "$LOG"
  exit 1
fi

echo "OK — app tournée ${ALIVE}s sous Xvfb/xcb sans crash"
grep -E '^\[|Updater|critical' "$LOG" | head -8 || true
