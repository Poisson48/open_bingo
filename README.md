# Open Bingo

Générateur de cartes bingo natif (Qt 6) pour soirées jeux de société.

## Fonctionnalités

- Projets multiples (CRUD, clone, recherche, export/import JSON)
- Configuration : taille grille, joueurs, HP, mode gage, multiplicateurs
- Génération de grilles par taux de probabilité
- Impression / PDF
- Mode sans papier (play)
- **Partage & sync** via relais Nostr chiffrés (stack [Colo Tâches](https://github.com/Poisson48/Colo_Taches))
- **Mises à jour auto** depuis GitHub Releases (APK Android + AppImage Linux)

## Build (Linux)

```bash
bash scripts/setup-dev.sh
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build
./build/src/openbingo
```

## Releases

Tag `v*` → GitHub Actions publie APK arm64 + AppImage :

```bash
git tag -a v2.0.0 -m "Première release Qt native" && git push origin v2.0.0
```

## Plateformes

| Plateforme | Statut |
|---|---|
| Linux (AppImage) | v1 |
| Android (APK) | v1 |
| Windows | phase 2 (`scripts/build-windows.ps1`) |

## Architecture

Voir [`docs/PLAN.md`](docs/PLAN.md) et [`docs/SPEC.md`](docs/SPEC.md).

Stack réseau : `net/` (Nostr/WebSocket), `cryptolayer/`, `ProjectSync`, `Updater`.

Licence : GPLv3
