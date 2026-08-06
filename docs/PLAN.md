# Open Bingo — Plan d'action

> Référence agents : lire ce fichier + `docs/SPEC.md`.

## Architecture

- Qt 6.8 / QML + C++20
- Stack réseau Colo Tâches : Nostr, crypto, pairing, ProjectSync, Updater
- SQLite locale + export JSON
- v1 : Linux AppImage + Android APK ; Windows phase 2

## Phases

| Phase | État |
|---|---|
| 0 Spec | ✅ |
| 1 CMake + core + store | ✅ |
| 2 UI principale | ✅ |
| 3 Print / Play / Share | ✅ |
| 4 CI + cleanup web | ✅ |
| 5 Windows | ✅ (script + workflow) |

## Release

Tag `v*` → `.github/workflows/release.yml` → APK + AppImage sur GitHub Releases.

