# open_bingo

Générateur de grilles de bingo personnalisables pour soirées jeux. Chaque joueur obtient une grille unique tirée aléatoirement depuis un pool de cases thématiques.

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
- **Gages** — liste de gages avec PV récupérés, imprimés sur une page dédiée (non tronquée)
- **Autosave localStorage** — toutes les modifications sont sauvegardées automatiquement ; l'état est restauré au rechargement
- **Export / Import JSON** — sauvegarde et restauration complète de la configuration
- **Aperçu impression A4** — 2 grilles joueurs par feuille + 1 page dédiée au tableau des gages
- **Détection de grilles obsolètes** — bannière d'avertissement si des phrases ont été modifiées depuis la dernière génération

## Lancer l'application

```bash
cd bingo-app
npm install
npm start
```

Ouvrir [http://localhost:3000](http://localhost:3000).

## Structure du projet

```
bingo-app/
  server.js           # Serveur Express (fichiers statiques uniquement)
  public/
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
