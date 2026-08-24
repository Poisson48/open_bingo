# Open Bingo — Spécification technique

## 1. Modèle Project (JSON)

Voir export web 1.2.6 — champs `id`, `title`, `description`, `gridSize`, `players`, `cases`, `gages`, `grids`, `gageMode`, `comboGages`, `multipliers`, etc.

## 2. SQLite

- `projects` — JSON complet par projet
- `settings` — `current_project_id`, `last_tab`, `device_id`
- `play_checks` — états cochés par joueur (hors export)
- `sync_keys` — clé de canal par projet partagé
- `outbox` / `seen_events` — file d'envoi Nostr et déduplication

## 3. Stack réseau (Colo Tâches)

- **Transport** : relais Nostr publics (`RelayPool`, kind 4545)
- **Chiffrement** : XChaCha20-Poly1305 (`net/crypto.cpp`, préfixe `open-bingo/v1/`)
- **Appairage** : `openbingo://join/1/<projectId>/<key>/<title>` (`core/pairing.cpp`)
- **Sync** : `ProjectSync` publie des snapshots JSON chiffrés ; merge LWW sur `updatedAt`
- **Mises à jour** : `Updater` interroge `api.github.com/repos/Poisson48/open_bingo/releases`

## 4. Génération de grilles

Identique app web — `rate=0` exclu strictement ; centre FREE si activé.

## 5. Impression

A4, 1 ou 2 grilles/page selon lisibilité des cases, feuille gages paginée (`PrintPage` + `QPrinter`).

## 6. Critères v1

- Build Linux + tests `ctest` verts
- Import JSON web sans perte
- Partage QR + sync Nostr entre 2 appareils
- Release GitHub : APK + AppImage
