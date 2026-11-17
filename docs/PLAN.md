# Open Bingo — Plan d'action

> Référence agents : lire ce fichier + [`PROTOCOL.md`](../PROTOCOL.md).

## Architecture

- Qt 6.8 / QML + C++20
- Stack réseau Colo v1 : relais Nostr chiffré colo-apps, crypto, pairing, ProjectSync, Updater
- Sans notifications push (app ouverte / premier plan)
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
| 6 CSV + MCP desktop | ✅ `feat/csv-mcp` — [`PLAN-csv-mcp.md`](PLAN-csv-mcp.md) |
| 7 OpenSubtitles / Bingo film | ✅ — [`PLAN-opensubtitles.md`](PLAN-opensubtitles.md) |
| 8 Refactor cleanup | ✅ warnings + découpe AppController/MCP/OpenSubtitles |
| 9 Réseau protocole v1 | ✅ multi-souscriptions, outbox/ACK, dédup, catch-up, rehost, sans push — voir [`PROTOCOL.md`](../PROTOCOL.md) |

## Release

Tag `v*` → `.github/workflows/release.yml` → APK + AppImage sur GitHub Releases.

