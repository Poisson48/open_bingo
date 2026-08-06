#!/usr/bin/env bash
# Smoke : ouvre le mode plein écran Play (délègue au multi-résolutions).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
exec bash "$ROOT/scripts/smoke-fullscreen-multi.sh" "${1:-/tmp/bingo-fs-smoke}"
