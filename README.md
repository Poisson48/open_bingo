# Open Bingo

Générateur de grilles de bingo personnalisables pour soirées jeux. Chaque joueur obtient une grille unique tirée aléatoirement depuis un pool de cases thématiques.

## Jouer en ligne (sans installation)

**[Ouvrir Open Bingo dans le navigateur →](https://poisson48.github.io/open_bingo/)**

Fonctionne directement dans Chrome, Firefox, Safari — aucune installation requise.

## Télécharger l'application

| Plateforme | Fichier | Notes |
|-----------|---------|-------|
| Linux | [`.AppImage`](https://github.com/Poisson48/open_bingo/releases/latest) | Toutes distros — `chmod +x` puis double-clic |
| Linux (Ubuntu/Debian) | [`.deb`](https://github.com/Poisson48/open_bingo/releases/latest) | `sudo dpkg -i fichier.deb` |
| Windows | [`.exe`](https://github.com/Poisson48/open_bingo/releases/latest) | Installeur NSIS |
| macOS | [`.dmg`](https://github.com/Poisson48/open_bingo/releases/latest) | Glisser dans Applications |
| Android | [`.apk`](https://github.com/Poisson48/open_bingo/releases/latest) | Activer "Sources inconnues" |

> Les données (projets) sont conservées entre les mises à jour — elles sont stockées dans le dossier utilisateur de l'app, indépendamment de la version installée.

## Aperçu

![Configuration](screenshots/01_config.png)

![Phrases & Gages](screenshots/02_phrases_gages.png)

![Grilles générées](screenshots/03_grilles.png)

![Aperçu impression](screenshots/04_print_grille.png)

![Tableau des Gages](screenshots/05_print_gages.png)

## Fonctionnalités

- **Gestion de plusieurs projets** — créez, clonez, modifiez et supprimez des bingos depuis la liste de projets
- **Recherche de projets** — barre de recherche sur le titre et la description
- **Export / Import** — exportez un projet ou tous les projets d'un coup (migration vers un autre PC)
- **Grilles uniques par joueur** — chaque grille est générée indépendamment avec mélange aléatoire
- **Taux d'apparition par case** — slider 0–100 % pour contrôler la probabilité d'inclusion
- **Taille de grille configurable** — de 2×2 à 12×12
- **Case FREE centrale** — activable si la grille est de taille impaire
- **Système de points & multiplicateurs** — ligne, colonne, diagonale, grille complète
- **Gages** — liste de défis avec PV récupérés, imprimés sur une page dédiée
- **Autosave** — toutes les modifications sont sauvegardées automatiquement
- **Aperçu impression A4** — 2 grilles par feuille + page dédiée au tableau des gages
- **App desktop native** — via Tauri v2 (Linux, Windows, macOS, Android)

## Lancer en local (mode serveur)

```bash
cd bingo-app
npm install
npm start        # → http://localhost:3000
```

## Build desktop (Tauri)

```bash
npm install               # installe @tauri-apps/cli
./scripts/build-app.sh    # → releases/vX.Y.Z/ (Linux uniquement en local)
npm run dev               # fenêtre native locale (hot-reload)
```

Les builds **Windows**, **macOS** et **Android** sont produits automatiquement par GitHub Actions lors d'un push de tag :

```bash
git tag v1.1.0
git push origin v1.1.0   # déclenche le workflow → GitHub Releases
```

Prérequis locaux (Linux) : Rust, `libwebkit2gtk-4.1-dev`, `patchelf`.

## Structure du projet

```
bingo-app/
  server.js           # Serveur Express (fichiers statiques uniquement)
  public/             # ← aussi servi via GitHub Pages
    index.html
    js/
      state.js        # État global, gestion des projets, autosave, export/import
      main.js         # Routage vues (projets / éditeur) + onglets
      projects.js     # Vue liste des projets
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
scripts/
  build-app.sh        # Build local Linux + copie dans releases/
.github/workflows/
  pages.yml           # Déploiement automatique sur GitHub Pages
  release.yml         # Build multi-plateforme sur tag git
```

## Utilisation

1. **Projets** — créer ou ouvrir un projet depuis la liste d'accueil
2. **Configuration** — définir le titre, la taille de grille, les joueurs et les multiplicateurs, puis cliquer *Enregistrer*
3. **Phrases & Gages** — ajouter les phrases (libellé, points, taux) et les gages (description, PV)
4. **Grilles** — générer les grilles pour tous les joueurs
5. **Aperçu impression** — imprimer ou exporter en PDF

## Tech

- Node.js + Express (serveur statique)
- Vanilla JS ES modules (aucun bundler, aucun framework)
- CSS natif
- Tauri v2 (desktop Linux / Windows / macOS + Android)
