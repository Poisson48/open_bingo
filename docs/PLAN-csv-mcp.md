# Plan — Import CSV + MCP desktop (phrases depuis sous-titres)

> Maj ciblée : préparer un bingo film **sans avoir vu le film**, via tableur et/ou IA (Cursor / Claude Desktop) branchée sur Open Bingo PC.

## Objectif produit

1. **CSV** — préparer phrases + gages dans un tableur à l’avance, puis importer (et exporter) dans un projet.
2. **MCP PC (HTTP localhost)** — serveur dans l’app desktop qui pilote le projet ouvert : list / create / bulk add.
3. **Workflow film** — l’IA lit un fichier de sous-titres SDH (malentendants : dialogues + bruitages), propose des phrases bingo, les écrit via MCP ; l’utilisateur affine rate / n° de gage, génère les grilles, sync téléphone.

Le MCP est **desktop uniquement** (AppImage / binaire Linux, plus tard Windows). Android reste client UI + sync Nostr.

## Parcours cible

```
Sous-titres .srt/.vtt (SDH)
        │
        ▼
IA (Cursor) ──MCP HTTP──► Open Bingo PC (GUI + tray)
        │                 │
        │                 ├── import CSV (manuel)
        │                 ├── add_case / add_gage
        │                 └── sync Nostr → téléphone
        ▼
Tableur (CSV) ◄── export / import UI
```

Exemple de prompt utilisateur :

> Ouvre le projet « Bingo Inception ». Lis `inception.sdh.srt`. Extrais ~80 répliques courtes et mémorables (pas les bruitages). Ajoute-les en phrases ; crée 5 gages (n° 1–5) et assigne les n° aux phrases selon l’intensité comique.

## État actuel (à réutiliser)

| Brique | Statut |
|---|---|
| `core/csv` (RFC 4180) | ✅ parse/write, **pas branché** bingo |
| `Case` / `Gage` (`bingotypes.h`) | ✅ `label/points/rate` · `description/hp/number/rate` |
| `AppController::addCase` / `addGage` | ✅ |
| Import/export JSON | ✅ |
| Sync Nostr | ✅ (téléphone reçoit les maj projet) |
| MCP / tray / import CSV UI | ❌ |

---

## Phase A — Format CSV + codec core

### A.1 Deux fichiers (décidé)

Séparer phrases et gages : simple à éditer dans Calc/Excel, mapping 1:1 avec le modèle. Pas de CSV unique `kind` en v1.

**`phrases.csv`**

```csv
label,points,rate
"On ne parle pas de Fight Club",1,80
"Je vois des gens morts",2,60
```

- `points` = points **ou** n° de gage si `gageMode` (comme l’UI actuelle).
- `rate` = 0–100 (0 = jamais tiré).
- Header obligatoire ; colonnes reconnues par nom (ordre libre). Alias acceptés : `phrase`/`texte` → `label`, `gage`/`numero` → `points`.

**`gages.csv`**

```csv
description,number,hp,rate
"Boire une gorgée",1,5,100
"Chanter 10 secondes",1,5,40
"Faire 10 pompes",2,10,100
```

### A.2 Variante « un seul fichier » (optionnelle, plus tard)

Colonne `kind` = `case` | `gage`. Utile pour l’IA en un seul write ; moins ergonomique tableur. V1 = deux fichiers.

### A.3 API C++

```cpp
// core/projectcsv.h
struct CsvImportResult { int added = 0; int skipped = 0; QStringList errors; };

CsvImportResult importPhrasesCsv(Project& p, const std::string& text, bool replace);
CsvImportResult importGagesCsv(Project& p, const std::string& text, bool replace);
std::string exportPhrasesCsv(const Project& p);
std::string exportGagesCsv(const Project& p);
```

- `replace=false` → append ; `true` → remplace la liste (comme « repartir d’un tableau propre »).
- Validation : label/description non vides ; rate clamp 0–100 ; number ≥ 1.
- Import / mutations MCP : **ne vident pas** `grids` ; posent `gridsDirty` (comme une édition manuelle). L’utilisateur régénère quand il veut.
- Tests `tst_csv_project` : round-trip, guillemets, BOM UTF-8, lignes vides, colonnes manquantes.

### A.4 UI (desktop + Android)

Sur **Cases** / **Gages** :

- Boutons **Importer CSV** / **Exporter CSV** (file picker desktop ; Android : `ACTION_GET_CONTENT` / partage).
- Dialog : « Remplacer » vs « Ajouter » + résumé `N ajoutées, M erreurs`.
- Raccourci menu projet : « Importer phrases+gages… » (deux fichiers ou dossier).

Pas de parsing SRT dans l’UI v1 : le fichier sous-titres reste côté IA / filesystem.

---

## Phase B — MCP desktop (HTTP, décidé)

### B.1 Forme du serveur

**Serveur MCP HTTP local dans le process GUI** (Streamable HTTP / SSE selon ce que Cursor accepte), démarré avec l’app PC.

- Même `AppController` / SQLite → mutations MCP = UI live (pas de process DB parallèle).
- Toggle Réglages (ou menu) : **MCP IA** on/off + URL + token local.
- Écoute **localhost uniquement** (ex. `127.0.0.1:4546`) ; token Bearer aléatoire affiché une fois / copiable.
- Désactivé hors desktop (Android compile out / no-op).

**Concurrence** : un seul process → pas de guerre SQLite. Après mutation MCP : mêmes signaux QML (`currentProjectChanged` / `gridsDirty`) + outbox sync si projet partagé.

### B.2 Transport & config Cursor

App ouverte + MCP activé → URL du type `http://127.0.0.1:4546/mcp`.

```json
{
  "mcpServers": {
    "openbingo": {
      "url": "http://127.0.0.1:4546/mcp",
      "headers": {
        "Authorization": "Bearer <token-affiché-dans-l-app>"
      }
    }
  }
}
```

Doc README : activer MCP dans l’app, copier le snippet, coller dans `.cursor/mcp.json`. Port configurable ; défaut `4546`.

### B.3 Outils MCP (v1)

| Tool | Rôle |
|---|---|
| `list_projects` | id, title, updatedAt, cases/gages counts |
| `get_project` | résumé config + aperçu phrases/gages (tronqué) |
| `create_project` | title, description → id |
| `open_project` | fixe le projet « courant » de la session MCP |
| `set_config` | gridRows/Cols, gageMode, freeCenter, startHP, players… |
| `add_cases` | bulk `[{label, points, rate}]` |
| `add_gages` | bulk `[{description, number, hp, rate}]` |
| `import_phrases_csv` / `import_gages_csv` | texte CSV ou chemin fichier local |
| `export_phrases_csv` / `export_gages_csv` | retourne le CSV |
| `clear_cases` / `clear_gages` | avec confirmation bool `confirm` |
| `generate_grids` | appelle le générateur existant |
| `project_stats` | minCases, availableCells, gridsDirty |

Hors scope v1 MCP : play checks, impression, QR (l’humain garde ça dans l’UI).

### B.4 Prompt / resource MCP (nice-to-have)

- Resource `openbingo://project/{id}/phrases` pour lecture.
- Prompt template `film_bingo_from_subtitles` : consignes (phrases courtes, pas de spoiler spoiler-heavy si demandé, dédup, rate par mémorabilité, SDH : ignorer `[bruit de porte]`).

### B.5 Sous-titres

**Pas d’outil « parse SRT » obligatoire en v1** : l’hôte MCP a déjà `Read` sur le `.srt`.  
Option v1.1 : `preview_subtitle_cues(path, max)` qui nettoie tags SDH / timestamps et renvoie un extrait compact → moins de tokens pour l’IA.

---

## Phase C — Fonctionnement « en arrière-plan » (PC)

Sens produit : **préparer / remplir un projet pendant que l’IA travaille**, sans bloquer la soirée ; sync vers les téléphones.

| Élément | v1 | v2 |
|---|---|---|
| MCP HTTP dans la GUI (localhost) | ✅ | — |
| GUI : minimiser dans la barre système (MCP reste up) | ✅ tray + « Continuer en arrière-plan » | — |
| Sync Nostr après mutations MCP | ✅ si projet partagé | — |
| MCP sans fenêtre (headless) | — | optionnel |
| Android background MCP | ❌ non | ❌ non |

Tray Linux (`QSystemTrayIcon`) : obligatoire pour le scénario « app en fond + Cursor qui remplit » — fermer la fenêtre ne doit **pas** tuer le serveur HTTP.

---

## Phase D — Doc & qualité

- README : section **CSV** + **MCP / bingo film**.
- Exemple `docs/examples/phrases.csv` + `gages.csv`.
- Skill Cursor optionnelle `.cursor/skills/openbingo-film-bingo/SKILL.md` (quand brancher le MCP, format CSV, prompt SDH).
- Tests : codec CSV, import replace/append, smoke MCP HTTP (`tools/list` + `add_cases` contre localhost).
- CI : test HTTP contre `AppController` headless / offscreen si possible ; sinon smoke manuel doc.

---

## Découpage d’implémentation

**Priorité : CSV + MCP en parallèle** (décidé) — les deux dès le premier fan-out, pas l’un après l’autre.

| Sprint | Livrable | Critère done |
|---|---|---|
| **0** | Contrats communs | `projectcsv.h` + liste tools MCP + interface `McpServer` figés |
| **1** | CSV **et** MCP HTTP (parallèle agents) | Import UI round-trip + Cursor `tools/list` / `add_cases` |
| **2** | Sync outbox + tray background + doc film | Projet partagé à jour sur téléphone ; MCP up en tray |
| **3** | Polish (prompt MCP, preview SRT, UX token) | Confort soirée |

Sprint 0 = orchestrateur seul (rapide). Sprint 1 = fan-out massif CSV ∥ MCP. Sync/tray ensuite.

---

## Méthode — multiplier les agents

Pour coder **vite et sans se marcher dessus** : un agent orchestrateur + plusieurs agents parallèles sur des **lots de fichiers disjoints**. Pas un seul agent long qui fait tout.

### Règles

1. **1 agent = 1 lot de fichiers** (pas de chevauchement `appcontroller.*` / même QML).
2. **Contrats d’abord** : l’orchestrateur (ou agent A) pose les headers / signatures (`projectcsv.h`, tools MCP list) avant le parallel fan-out.
3. **Lancer en parallèle** dès que les contrats sont mergés (Task / best-of-n / chats séparés).
4. **Agent intégration** en fin de sprint : CMake, `ctest`, collages UI↔controller, smoke HTTP MCP.
5. **Branche `feat/csv-mcp`** unique ; sous-lots = commits ou worktrees, pas de rewrite croisé.
6. Relancer un agent sur un lot seulement s’il est **bloqué ou raté** — pas 5 agents sur le même bug.
7. **Toujours** un agent **garde-fou** (qualité, tests, contrats, régressions) et un agent **UI/UX** (cohérence visuelle Colo, accessibilité, copy) — en parallèle du fan-out, pas en option.

### Fan-out Sprint 1 — CSV ∥ MCP (priorité double)

Après Sprint 0 (contrats), lancer **tous** ces lots en parallèle :

| Agent | Scope (fichiers) | Livrable |
|---|---|---|
| **B — codec CSV** | `core/projectcsv.cpp`, `tests/tst_csv_project*` | Round-trip + edge cases |
| **C — controller CSV** | `appcontroller.*` (méthodes CSV only) | `import/export*Csv` + `gridsDirty` |
| **E — exemples CSV** | `docs/examples/*.csv`, snippets README CSV | Artefacts doc |
| **F — bootstrap HTTP** | `app/mcpserver.*` (listen/auth/JSON-RPC) | Listen localhost + Bearer |
| **G+H — tools MCP** | handlers dans `mcpserver.cpp` seulement | tools read+write via AppController |
| **UI/UX** (dédié, toujours) | `CasesPage`, `GagesPage`, `ConfigPage` MCP, dialogs | Import/export CSV + panneau MCP Colo |
| **Garde-fou** (dédié, toujours) | tests, revue contrats, `tst_appflow` CSV, smoke HTTP | Bloque merge si contrat cassé |

**A/Sprint 0** pose `projectcsv.h` + signatures tools + squelette `McpServer`.  
**C** et **G+H** : écriture uniquement via AppController (MCP n’écrit pas la DB direct).  
**UI/UX** seul touche le QML de cette maj. **Garde-fou** ne feature-pas : il vérifie, corrige tests, signale.

### Fan-out Sprint 2–3

| Agent | Scope |
|---|---|
| **J** | Outbox / sync après mutations MCP (déjà dans process GUI) |
| **K** | Tray : fermer fenêtre ≠ quit si MCP on |
| **L** | Skill film-bingo + prompt MCP + (option) preview SRT |
| **M** | Intégration CI / smoke e2e |

**J ∥ K ∥ L** ; **M** ferme.

### Prompt type pour un agent fils

> Lot **B — codec CSV**. Lis `docs/PLAN-csv-mcp.md` § Phase A. N’édite que `core/projectcsv.cpp` et `tests/tst_csv_project*`. Respecte l’API déjà dans `projectcsv.h`. Pas de QML, pas de MCP. `ctest` vert sur ton test.

L’orchestrateur garde le plan à jour (cases cochées) et fusionne ; les fils ne renegocient pas le format CSV / la liste des tools.

## Hors scope (volontaire)

- Transcription audio / vision du film (l’IA lit des **sous-titres déjà fournis**).
- Hébergement cloud de MCP.
- Édition collaborative temps réel case-à-case (toujours LWW snapshot Nostr).
- Remplacer le JSON d’échange entre versions web historiques.

## Risques & mitigations

| Risque | Mitigation |
|---|---|
| Cursor n’aime pas le transport HTTP choisi | Prototyper Streamable HTTP dès Sprint 0 ; fallback SSE documenté |
| Token / port oublié | UI « Copier config MCP » one-click |
| App tuée → MCP down | Tray : hide ≠ quit tant que MCP on |
| IA qui dump trop de lignes SDH | Prompt + `project_stats` + limite soft `add_cases` (warn > 500) |
| Spoils / phrases trop longues | Consignes prompt ; rate défaut 50 |

## Décisions figées

| # | Choix |
|---|---|
| 1 | **Deux CSV** : `phrases.csv` + `gages.csv` |
| 2 | **MCP HTTP** localhost dans la GUI PC (pas stdio CLI) |
| 3 | Après import / bulk MCP : **grilles conservées + `gridsDirty`** (pas de wipe) |
| 4 | **Priorité double** : CSV UI et MCP dès le Sprint 1 (parallèle agents) |

---

## Lien avec le plan global

| Phase | Contenu |
|---|---|
| 6 CSV + MCP desktop | import tableur + serveur MCP HTTP + workflow sous-titres |
| 7 OpenSubtitles + Bingo film | recherche film → SRT SDH → phrases (+ grosse UX) — [`PLAN-opensubtitles.md`](PLAN-opensubtitles.md) |
