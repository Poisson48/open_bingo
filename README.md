# Open Bingo

<p align="center">
  <img src="docs/assets/logo.png" alt="Open Bingo" width="120">
</p>

<p align="center">
  <strong>Générateur de cartes bingo pour soirées jeux — natif, gratuit, sans serveur à héberger.</strong>
</p>

<p align="center">
  <a href="https://poisson48.github.io/open_bingo/"><strong>Site web & téléchargements</strong></a> ·
  <a href="https://github.com/Poisson48/open_bingo/releases/latest">Releases</a> ·
  <a href="https://github.com/Poisson48/open_bingo/actions/workflows/ci.yml"><img src="https://github.com/Poisson48/open_bingo/actions/workflows/ci.yml/badge.svg" alt="CI"></a>
</p>

Créez des projets bingo, des grilles uniques par joueur, imprimez-les ou jouez sans papier sur téléphone. Partagez un projet entre plusieurs appareils par QR code : les modifications se synchronisent via relais Nostr chiffrés. Données locales en SQLite, export/import JSON. Open source, GPLv3.

> **v2.x** — application **Qt 6 / QML** native (Linux AppImage + Android APK).  
> Les releases **v1.2.x** (app web/Tauri) ne sont **pas** compatibles (package Android différent).

## Aperçu

<p align="center">
  <img src="docs/assets/screenshots/phone/05-play.png" alt="Mode play" width="280">
  &nbsp;
  <img src="docs/assets/screenshots/phone/01-projects.png" alt="Projets" width="280">
  &nbsp;
  <img src="docs/assets/screenshots/phone/04-grids.png" alt="Grilles" width="280">
</p>

<p align="center">
  <img src="docs/assets/screenshots/phone/02-config.png" alt="Configuration" width="220">
  &nbsp;
  <img src="docs/assets/screenshots/phone/03-cases.png" alt="Phrases" width="220">
</p>

<p align="center"><em>Interface sombre indigo — responsive mobile & desktop · <a href="https://poisson48.github.io/open_bingo/#screenshots">plus de captures</a></em></p>

## Télécharger

**[→ Page de téléchargement](https://poisson48.github.io/open_bingo/#download)** · **[Releases GitHub](https://github.com/Poisson48/open_bingo/releases/latest)**

| Plateforme | Fichier | Installation |
|---|---|---|
| **PC / Linux** (x86-64) | `OpenBingo-*-x86_64.AppImage` | `chmod +x OpenBingo-*.AppImage && ./OpenBingo-*.AppImage` |
| **Android** (arm64) | `openbingo-*-arm64.apk` | Ouvrez l'APK sur le téléphone, ou `adb install -r openbingo-*-arm64.apk` |

L'AppImage embarque Qt : un seul fichier, aucune dépendance système.

Sur Android, l'app vérifie les mises à jour sur GitHub au lancement. Les releases signées s'installent **par-dessus** la version précédente.

## Fonctionnalités

- **Projets multiples** — créer, cloner, rechercher, supprimer ; métadonnées titre/description
- **Configuration** — taille de grille (2–12), joueurs, HP de départ, case FREE au centre, mode gage
- **Phrases & gages** — pool de cases avec points et taux d'inclusion (0–100 %)
- **Grilles** — génération aléatoire par joueur, reshuffle, édition manuelle
- **Impression** — aperçu A4 (2 grilles/page) + page gages
- **Sans papier** — cocher les cases, score en direct, plein écran paysage, persistance des checks
- **Partage & sync** — QR / lien `openbingo://join/…`, chiffrement bout en bout, sync Nostr
- **Import/export JSON** — projet seul ou tous les projets ; compatible exports web v1.2.x

## Comment ça marche (sync)

1. Créez ou ouvrez un projet sur un appareil.
2. Menu **Partager** → QR code ou lien d'invitation (contient la clé de chiffrement).
3. L'autre appareil scanne le QR ou ouvre le lien → rejoint le canal.
4. Chaque modification est publiée chiffrée sur des relais Nostr publics ; l'autre appareil fusionne au retour en ligne (dernier `updatedAt` gagne).

Hors ligne : les changements partent dans une file d'attente locale et sont envoyés au retour du réseau.

## Développement

### Prérequis (Ubuntu / Debian)

```bash
bash scripts/setup-dev.sh   # sudo requis — installe Qt 6, libsodium, libsecp256k1, etc.
```

### Build & tests

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
./build/src/openbingo
```

Captures d'écran pour la doc :

```bash
bash scripts/capture-screenshots.sh   # → docs/assets/screenshots/
```

### Android (APK local)

```bash
bash scripts/setup-android.sh    # une fois (~7 Go, SDK + NDK + Qt cross)
bash scripts/build-android.sh
# → openbingo-arm64.apk
```

### AppImage (Linux)

```bash
bash scripts/build-appimage.sh
```

## Publier une release

Chaque tag `v*` déclenche GitHub Actions (`release.yml`) : build APK arm64 + AppImage, publication sur GitHub Releases.

```bash
git tag -a v2.0.5 -m "Description des nouveautés (affichée dans l'app avant mise à jour)"
git push origin v2.0.5
```

**Secrets GitHub** (optionnels mais recommandés pour Android) :

| Secret | Rôle |
|---|---|
| `ANDROID_KEYSTORE_B64` | Keystore de publication (base64) |
| `ANDROID_KEY_ALIAS` | Alias de la clé |
| `ANDROID_KEYSTORE_PASS` | Mot de passe |

Générer une clé une fois : `bash scripts/make-release-key.sh`

## Architecture

```
src/
  core/     Générateur, JSON, pairing, CRDT
  net/      Nostr, WebSocket, chiffrement
  store/    SQLite (projets, sync, play checks)
  app/      AppController, ProjectSync, Updater, Theme
  qml/      Interface Material (pages, composants Bingo*)
```

Documentation : [`docs/PLAN.md`](docs/PLAN.md), [`docs/SPEC.md`](docs/SPEC.md), [`CLAUDE.md`](CLAUDE.md).

## Plateformes

| Plateforme | Statut |
|---|---|
| Linux x86-64 (AppImage) | ✅ |
| Android arm64 (APK) | ✅ |
| Windows | 🚧 script + CI préparés |

## Licence

[GNU GPLv3](LICENSE)
