#!/usr/bin/env bash
# Build l'AppImage + .deb et copie dans releases/vX.Y.Z/
# Usage : ./scripts/build-app.sh [version]
#   Sans argument → incrémente automatiquement le patch (1.0.0 → 1.0.1)

set -e

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT"

CONF="src-tauri/tauri.conf.json"
CURRENT=$(node -p "require('./$CONF').version")

if [ -n "$1" ]; then
  VERSION="$1"
else
  # Auto-bump patch : 1.0.3 → 1.0.4
  IFS='.' read -r MAJOR MINOR PATCH <<< "$CURRENT"
  VERSION="$MAJOR.$MINOR.$((PATCH + 1))"
fi

DEST="$REPO_ROOT/releases/v$VERSION"

if [ -d "$DEST" ]; then
  echo "⚠  releases/v$VERSION existe déjà. Précise une autre version :"
  echo "   ./scripts/build-app.sh $MAJOR.$MINOR.$((PATCH + 2))"
  exit 1
fi

echo "→ $CURRENT  ➜  $VERSION"

# Patch tauri.conf.json
node -e "
  const fs = require('fs');
  const c = JSON.parse(fs.readFileSync('$CONF'));
  c.version = '$VERSION';
  fs.writeFileSync('$CONF', JSON.stringify(c, null, 2) + '\n');
"

echo "→ Build Tauri…"
npm run build

BUNDLE="$REPO_ROOT/src-tauri/target/release/bundle"
mkdir -p "$DEST"

find "$BUNDLE/appimage" "$BUNDLE/deb" -type f 2>/dev/null | while read f; do
  cp "$f" "$DEST/"
done

echo ""
echo "✓  releases/v$VERSION :"
ls -lh "$DEST/"
echo ""
echo "   Lancer : \"$DEST/Open Bingo_${VERSION}_amd64.AppImage\""
