# Open Bingo

Générateur de grilles de bingo personnalisables pour soirées jeux. Chaque joueur obtient une grille unique tirée aléatoirement depuis un pool de cases thématiques.

## Jouer en ligne (sans installation)

**[Ouvrir Open Bingo dans le navigateur →](https://poisson48.github.io/open_bingo/)**

Fonctionne directement dans Chrome, Firefox, Safari — aucune installation requise.

## Télécharger l'application

| Plateforme | Fichier | Notes |
|-----------|---------|-------|
| Linux | [`.AppImage`](https://github.com/Poisson48/open_bingo/releases/latest) | Toutes distros — `chmod +x` puis double-clic |
| Linux (Ubuntu/Debian) | [`.deb`](https://github.com/Poisson48/open_bingo/releases/latest) | `sudo dpkg -i` |
| Android | [`.apk`](https://github.com/Poisson48/open_bingo/releases/latest) | Activer "Sources inconnues" |
| Windows | — | Build via GitHub Actions (à venir) |

## Aperçu

![Configuration](screenshots/01_config.png)

![Phrases & Gages](screenshots/02_phrases_gages.png)

![Grilles générées](screenshots/03_grilles.png)

![Aperçu impression](screenshots/04_print_grille.png)

![Tableau des Gages](screenshots/05_print_gages.png)

## Fonctionnalités

- **Grilles uniques par joueur** — chaque grille est générée indépendamment avec un mélange aléatoire
- **Taux d'apparition par case** — slider 0–100 % (par dizaine) pour contrôler la probabilité d'inclusion ; `rate=0` garantit l'exclusion totale
- **Taille de grille configurable** — de 2×2 à 12×12
- **Case FREE centrale** — activable uniquement si la grille est de taille impaire
- **Système de points & multiplicateurs** — ligne, colonne, diagonale, grille complète
- **Joueurs configurables** — ajout/suppression de joueurs depuis l'interface
- **Gages** — liste de gages avec PV récupérés, imprimés sur une page dédiée
- **Autosave localStorage** — toutes les modifications sont sauvegardées automatiquement
- **Export / Import JSON** — sauvegarde et restauration complète de la configuration
- **Aperçu impression A4** — 2 grilles joueurs par feuille + 1 page dédiée au tableau des gages
- **Détection de grilles obsolètes** — bannière d'avertissement si des phrases ont été modifiées depuis la dernière génération
- **App desktop native** — via Tauri v2 (Linux, Android ; Windows à venir)

## Lancer en local (mode serveur)

```bash
cd bingo-app
npm install
npm start
```

Ouvrir [http://localhost:3000](http://localhost:3000).

## Build desktop (Tauri)

```bash
npm install          # installe @tauri-apps/cli
npm run build        # → .deb + .AppImage dans src-tauri/target/release/bundle/
npm run android      # → .apk dans src-tauri/gen/android/app/build/outputs/
npm run dev          # fenêtre native locale (hot-reload)
```

Prérequis : Rust, `libwebkit2gtk-4.1-dev`, `patchelf`, Android SDK + NDK 28.

## Structure du projet

```
bingo-app/
  server.js           # Serveur Express (fichiers statiques uniquement)
  public/             # ← aussi servi via GitHub Pages
    index.html
    js/
      state.js        # État global, autosave localStorage, export/import JSON
      main.js         # Routage des onglets
      config.js       # Onglet configuration
      cases.js        # Onglet phrases & gages
      generator.js    # Logique de génération des grilles
      grids.js        # Onglet affichage des grilles
      print.js        # Onglet aperçu impression
      ui.js           # Utilitaires (toast)
    style/
      main.css
      dashboard.css
      grid.css
      print.css
src-tauri/            # Tauri v2 — binaires natifs
.github/workflows/
  pages.yml           # Déploiement automatique sur GitHub Pages
```

## Utilisation

1. **Configuration** — définir le titre, la taille de grille, les joueurs et les multiplicateurs, puis cliquer *Enregistrer*
2. **Phrases & Gages** — ajouter/modifier les phrases (libellé, points, taux) et les gages (description, PV)
3. **Grilles** — générer les grilles pour tous les joueurs (bouton *Générer toutes les grilles*)
4. **Aperçu impression** — imprimer ou exporter en PDF (2 grilles/page + page gages séparée)

## Thème par défaut

Le projet inclut un thème **Bingo Complotiste — Da Vinci Code** avec 30 phrases thématiques et 10 gages, utilisable directement ou comme exemple pour créer ses propres configurations.

## Tech

- Node.js + Express (serveur statique)
- Vanilla JS ES modules (aucun bundler, aucun framework)
- CSS natif
- Tauri v2 (desktop Linux/Windows + Android)
