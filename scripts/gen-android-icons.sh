#!/usr/bin/env bash
# Génère les mipmaps Android depuis packaging/openbingo.png
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/packaging/openbingo.png"
OUT="$ROOT/android/res"
BG="#C62828"

[ -f "$SRC" ] || { echo "Logo absent : $SRC" >&2; exit 1; }
command -v convert >/dev/null || { echo "ImageMagick requis (convert)" >&2; exit 1; }

gen() {
    local dir="$1" launcher="$2" foreground="$3"
    mkdir -p "$OUT/mipmap-$dir"
    convert -size "${launcher}x${launcher}" "xc:$BG" \
        \( "$SRC" -resize "$((launcher * 7 / 10))x$((launcher * 7 / 10))" \) \
        -gravity center -composite \
        "$OUT/mipmap-$dir/ic_launcher.png"
    convert -size "${foreground}x${foreground}" "xc:none" \
        \( "$SRC" -resize "$((foreground * 7 / 10))x$((foreground * 7 / 10))" \) \
        -gravity center -composite \
        "$OUT/mipmap-$dir/ic_launcher_foreground.png"
    cp "$OUT/mipmap-$dir/ic_launcher.png" "$OUT/mipmap-$dir/ic_launcher_round.png"
}

gen mdpi 48 108
gen hdpi 72 162
gen xhdpi 96 216
gen xxhdpi 144 324
gen xxxhdpi 192 432

echo "Icônes Android générées dans $OUT/mipmap-*"
