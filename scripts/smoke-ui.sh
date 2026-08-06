#!/usr/bin/env bash
# Vérifie que l'app démarre et que le QML se charge sans erreur fatale.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$ROOT/build/src/openbingo"

[ -x "$BIN" ] || { echo "Binaire absent : $BIN" >&2; exit 1; }

echo "== smoke UI =="
OUTPUT="$(QT_QPA_PLATFORM=offscreen timeout 5 "$BIN" 2>&1 || true)"

FAIL_PATTERNS=(
    "QQmlApplicationEngine failed to load"
    "is not a type"
    "is not installed"
    "Cannot open: qrc:/icons"
    "Type .* unavailable"
)

for pat in "${FAIL_PATTERNS[@]}"; do
    if echo "$OUTPUT" | grep -qE "$pat"; then
        echo "$OUTPUT"
        echo "ÉCHEC : pattern « $pat » détecté" >&2
        exit 1
    fi
done

echo "OK — QML chargé, pas d'erreur fatale détectée"
# Afficher les lignes utiles (réseau/updater) sans bruit excessif
echo "$OUTPUT" | grep -E '^\[|Updater|critical|error' || true
