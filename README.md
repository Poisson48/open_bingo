# open_bingo

Générateur de grilles de bingo personnalisables pour soirées jeux. Chaque joueur obtient une grille unique tirée aléatoirement depuis un pool de cases thématiques.

## Fonctionnalités

- **Grilles uniques par joueur** — chaque grille est générée indépendamment avec un mélange aléatoire
- **Taux d'apparition par case** — chaque case a une probabilité (0–100 %) d'être incluse dans les grilles
- **Taille de grille configurable** — de 2×2 à 12×12
- **Case FREE centrale** — activable si la grille est de taille impaire
- **Système de points & multiplicateurs** — ligne, colonne, diagonale, grille complète
- **Joueurs configurables** — ajout/suppression de joueurs depuis l'interface
- **Gages** — liste de gages avec coût en points de vie
- **Export / Import JSON** — sauvegarde et restauration complète de la configuration
- **Aperçu impression** — mise en page dédiée pour imprimer les grilles

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
      state.js        # État global, export/import JSON
      main.js         # Routage des onglets
      config.js       # Onglet configuration
      cases.js        # Onglet gestion des cases
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

1. **Configuration** — définir le titre, la taille de grille, les joueurs et les multiplicateurs
2. **Cases** — ajouter/modifier les cases avec leur libellé, valeur en points et taux d'apparition
3. **Grilles** — générer les grilles pour tous les joueurs (bouton « Générer »)
4. **Aperçu impression** — imprimer ou exporter les grilles au format PDF

## Thème par défaut

Le projet inclut un thème « Bingo Complotiste — Da Vinci Code » avec 30 cases thématiques et 10 gages, utilisable directement ou comme exemple pour créer ses propres configurations.

## Tech

- Node.js + Express (serveur statique)
- Vanilla JS ES modules (aucun bundler, aucun framework)
- CSS natif
