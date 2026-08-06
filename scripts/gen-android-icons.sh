#!/usr/bin/env bash
# Génère les mipmaps Android depuis packaging/openbingo.png (logo bingo original).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/packaging/openbingo.png"
OUT="$ROOT/android/res"

[ -f "$SRC" ] || { echo "Logo absent : $SRC" >&2; exit 1; }
command -v convert >/dev/null || { echo "ImageMagick requis (convert)" >&2; exit 1; }

gen() {
    local dir="$1" launcher="$2" foreground="$3"
    mkdir -p "$OUT/mipmap-$dir"
    convert "$SRC" -resize "${launcher}x${launcher}" \
        "$OUT/mipmap-$dir/ic_launcher.png"
    convert "$SRC" -resize "$((foreground * 72 / 108))x$((foreground * 72 / 108))" \
        -background none -gravity center -extent "${foreground}x${foreground}" \
        "$OUT/mipmap-$dir/ic_launcher_foreground.png"
    cp "$OUT/mipmap-$dir/ic_launcher.png" "$OUT/mipmap-$dir/ic_launcher_round.png"
}

gen mdpi 48 108
gen hdpi 72 162
gen xhdpi 96 216
gen xxhdpi 144 324
gen xxxhdpi 192 432

echo "Icônes Android générées dans $OUT/mipmap-*"
