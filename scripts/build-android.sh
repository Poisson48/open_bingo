#!/usr/bin/env bash
# Build Android APK et copie dans releases/vX.Y.Z/
# Usage : ./scripts/build-android.sh [version]
# Prérequis : Android SDK, NDK, Java, Rust + targets android installés

set -e

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT"

# ── Vérifications ──────────────────────────────────────────────────────────────

if [ -z "$ANDROID_HOME" ]; then
  echo "❌  ANDROID_HOME n'est pas défini."
  echo "    Exemple : export ANDROID_HOME=~/android-sdk"
  exit 1
fi

NDK_HOME="${NDK_HOME:-$(ls -d "$ANDROID_HOME/ndk/"* 2>/dev/null | sort -V | tail -1)}"
if [ -z "$NDK_HOME" ] || [ ! -d "$NDK_HOME" ]; then
  echo "❌  NDK introuvable dans $ANDROID_HOME/ndk/"
  echo "    Installe-le : \$ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager 'ndk;28.2.13676358'"
  exit 1
fi

if ! command -v java &>/dev/null; then
  echo "❌  Java introuvable — installe JDK 17 ou 21."
  exit 1
fi

MISSING_TARGETS=""
for T in aarch64-linux-android armv7-linux-androideabi i686-linux-android x86_64-linux-android; do
  rustup target list --installed | grep -q "$T" || MISSING_TARGETS="$MISSING_TARGETS $T"
done
if [ -n "$MISSING_TARGETS" ]; then
  echo "❌  Targets Rust Android manquants :$MISSING_TARGETS"
  echo "    Installe-les : rustup target add$MISSING_TARGETS"
  exit 1
fi

# ── Version ────────────────────────────────────────────────────────────────────

CONF="src-tauri/tauri.conf.json"
CURRENT=$(node -p "require('./$CONF').version")

if [ -n "$1" ]; then
  VERSION="$1"
else
  IFS='.' read -r MAJOR MINOR PATCH <<< "$CURRENT"
  VERSION="$MAJOR.$MINOR.$((PATCH + 1))"
fi

DEST="$REPO_ROOT/releases/v$VERSION"

if [ -d "$DEST" ] && ls "$DEST"/*.apk &>/dev/null 2>&1; then
  echo "⚠  Un APK existe déjà dans releases/v$VERSION"
  echo "   Précise une autre version : ./scripts/build-android.sh $MAJOR.$MINOR.$((PATCH + 2))"
  exit 1
fi

echo "→ Version : $CURRENT  ➜  $VERSION"
echo "→ NDK     : $NDK_HOME"
echo "→ Java    : $(java -version 2>&1 | head -1)"

# ── Patch version ──────────────────────────────────────────────────────────────

node -e "
  const fs = require('fs');
  const c = JSON.parse(fs.readFileSync('$CONF'));
  c.version = '$VERSION';
  fs.writeFileSync('$CONF', JSON.stringify(c, null, 2) + '\n');
"

export ANDROID_HOME
export NDK_HOME
export JAVA_HOME="${JAVA_HOME:-$(dirname "$(dirname "$(readlink -f "$(which java)")")")}"

# ── Build ──────────────────────────────────────────────────────────────────────

echo "→ Build Android APK…"
npm run android -- build --apk

# ── Copie dans releases/ ───────────────────────────────────────────────────────

mkdir -p "$DEST"
APK_DIR="src-tauri/gen/android/app/build/outputs/apk"

find "$APK_DIR" -name "*.apk" | while read apk; do
  cp "$apk" "$DEST/"
done

echo ""
echo "✓  releases/v$VERSION :"
ls -lh "$DEST/"*.apk 2>/dev/null || echo "   (aucun APK trouvé — vérifie les logs ci-dessus)"
