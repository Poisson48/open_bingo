# open_bingo — CLAUDE.md

## Project overview

Native Qt 6 bingo card generator for tabletop game nights. Multi-project, rate-based grid
generation, print and paperless play modes. Replaces the former web/Tauri app entirely.

**Start here:** [`docs/PLAN.md`](docs/PLAN.md) + [`docs/SPEC.md`](docs/SPEC.md)

## Stack

- **UI**: Qt 6 Quick / QML (Material style)
- **Logic**: C++20 static libs (`core`, `store`, `applib`)
- **Build**: CMake 3.24+, Ninja
- **Persistence**: SQLite + JSON export/import
- **Réseau** : Nostr + chiffrement (stack Colo Tâches), `ProjectSync`, `Updater`

## Running the app

```bash
cmake -S . -B build -G Ninja
cmake --build build
./build/src/openbingo
```

Android / AppImage: see `scripts/` and `README.md`.

## Architecture

```
src/
  core/       bingotypes, Generator, JsonCodec, pairing, payload, crdt
  net/        Nostr, RelayPool, crypto
  store/      Database (SQLite + sync outbox)
  app/        AppController, ProjectSync, Updater, Theme, Platform
  qml/        Main.qml, pages, Colo* components
  main.cpp
tests/        generator, json, database, smoke
```

## Key behaviors

- **Rate-based generation**: each case has `rate` (0–100%). `rate=0` never included.
- **Config save resets grids**: explicit save on config tab clears `grids`.
- **Play checks** stored separately from project JSON.
- **Sync projets partagés** : snapshots JSON chiffrés via relais Nostr ; merge LWW sur `updatedAt`.
- **Mises à jour** : GitHub Releases → APK (Android) / AppImage (Linux).

## Dev conventions

- Follow Colo Course patterns: context property `Theme`, `StackView` navigation, `ColoDialog`.
- QML module URI: `OpenBingo`.
- Conventional commits, `feat/<slug>` branches, local build + ctest before merge.
