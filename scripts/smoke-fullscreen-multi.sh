#!/usr/bin/env bash
# Smoke multi-résolutions du mode play plein écran + vérif pixels (grille bord à bord).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$ROOT/build/src/openbingo"
OUT="${1:-/tmp/bingo-fs-multi}"

[ -x "$BIN" ] || { echo "Build requis" >&2; exit 1; }
command -v xvfb-run >/dev/null || { echo "xvfb-run requis" >&2; exit 1; }

rm -rf "$OUT"
mkdir -p "$OUT"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

SIZES=(
  "640x360"
  "800x360"
  "854x480"
  "1280x720"
)

fail=0
for size in "${SIZES[@]}"; do
  w="${size%x*}"
  h="${size#*x}"
  dir="$OUT/${w}x${h}"
  mkdir -p "$dir"
  echo "== Fullscreen ${w}×${h} =="
  timeout 45 xvfb-run -a -s "-screen 0 ${w}x${h}x24" env \
    XDG_DATA_HOME="$TMP/data-$w$h" \
    BINGO_TEST_W="$w" BINGO_TEST_H="$h" \
    BINGO_SCREENSHOT_DIR="$dir" \
    QT_QPA_PLATFORM=xcb \
    "$BIN" 2>&1 | tee "$TMP/log-$w$h.txt" | grep -E 'Screenshot:|TypeError|ReferenceError|unavailable' || true

  shot="$dir/06-play-fullscreen.png"
  if [ ! -f "$shot" ]; then
    echo "FAIL: $shot manquant" >&2
    grep -E 'error|Error|unavailable|failed' "$TMP/log-$w$h.txt" >&2 || true
    fail=1
    continue
  fi
  info="$(file "$shot")"
  echo "  $info"
  if ! echo "$info" | grep -q "${w} x ${h}"; then
    echo "FAIL: résolution capture inattendue (attendu ${w}x${h})" >&2
    fail=1
  fi

  if ! python3 - "$shot" "$w" "$h" <<'PY'
import sys
try:
    from PIL import Image
except ImportError:
    print("FAIL: module PIL manquant (installer python3-pil)", file=sys.stderr)
    sys.exit(2)
path, ew, eh = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
im = Image.open(path).convert("RGB")
w, h = im.size
assert w == ew and h == eh, (w, h, ew, eh)
px = im.load()

def content(rgb):
    r, g, b = rgb
    return not (r < 40 and g < 50 and b < 60)

y = min(h - 1, max(45, h // 5))
left = next((x for x in range(w) if content(px[x, y])), None)
right = next((x for x in range(w - 1, -1, -1) if content(px[x, y])), None)
assert left is not None and right is not None, "pas de contenu grille"
span = right - left
assert span >= int(w * 0.92), f"grille trop étroite: span={span} w={w}"
yb = h - 6
assert any(content(px[x, yb]) for x in range(0, w, max(1, w // 40))), "bas d'écran vide"
tr = [px[x, yy] for yy in range(4, 32) for x in range(w - 42, w - 6)]
bright = sum(1 for r, g, b in tr if r + g + b > 180)
assert bright >= 20, f"bouton fermer absent (bright={bright})"
print(f"  OK pixels: span={span}/{w} close_bright={bright}")
PY
  then
    echo "FAIL: vérification pixels" >&2
    fail=1
  fi
done

if [ "$fail" -ne 0 ]; then
  echo "ÉCHEC smoke fullscreen multi" >&2
  exit 1
fi
echo "OK — captures dans $OUT"
