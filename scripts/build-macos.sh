#!/usr/bin/env bash
# Build macOS (.dmg) et copie dans releases/vX.Y.Z/
# Usage : ./scripts/build-macos.sh [version]
# Prérequis : Rust, Xcode Command Line Tools — executer sur macOS

set -e

if [[ "$(uname)" != "Darwin" ]]; then
  echo "❌  Ce script doit être exécuté sur macOS."
  exit 1
fi

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT"

CONF="src-tauri/tauri.conf.json"
CURRENT=$(node -p "require('./$CONF').version")

if [ -n "$1" ]; then
  VERSION="$1"
else
  IFS='.' read -r MAJOR MINOR PATCH <<< "$CURRENT"
  VERSION="$MAJOR.$MINOR.$((PATCH + 1))"
fi

DEST="$REPO_ROOT/releases/v$VERSION"

if ls "$DEST"/*.dmg &>/dev/null 2>&1; then
  echo "⚠  Un .dmg existe déjà dans releases/v$VERSION"
  exit 1
fi

echo "→ Version : $CURRENT  ➜  $VERSION"

node -e "
  const fs = require('fs');
  const c = JSON.parse(fs.readFileSync('$CONF'));
  c.version = '$VERSION';
  c.bundle.targets = ['dmg'];
  fs.writeFileSync('$CONF', JSON.stringify(c, null, 2) + '\n');
"

echo "→ Build macOS..."
npm run build

mkdir -p "$DEST"
find "src-tauri/target/release/bundle/dmg" -name "*.dmg" | while read f; do
  cp "$f" "$DEST/"
done

echo ""
echo "✓  releases/v$VERSION :"
ls -lh "$DEST/"*.dmg
