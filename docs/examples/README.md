# Exemples CSV — bingo film (SDH → Open Bingo)

Préparer un bingo **sans avoir vu le film** : sous-titres SDH (fichier local, OpenSubtitles PC, ou IA) → projet Open Bingo.

## Fichiers

| Fichier | Colonnes | Rôle |
|---|---|---|
| [`phrases.csv`](phrases.csv) | `label`, `points`, `rate` | Répliques / situations à cocher |
| [`gages.csv`](gages.csv) | `description`, `number`, `hp`, `rate` | Pénalités liées au n° de gage |

- UTF-8, header obligatoire, RFC 4180 (guillemets si virgule).
- `points` = points **ou** n° de gage si mode gage activé.
- `rate` = 0–100 (0 = jamais tiré). Alias possibles : `phrase`/`texte` → `label`, `gage`/`numero` → `points`.
- Détail du format : [`docs/PLAN-csv-mcp.md`](../PLAN-csv-mcp.md).

## Workflow manuel (CSV)

1. Extraire des répliques mémorables depuis un `.srt` / `.vtt` SDH (ignorer `[bruitages]`).
2. Remplir `phrases.csv` + `gages.csv` (Calc / Excel / LibreOffice).
3. Dans Open Bingo : **Importer CSV** (ajouter ou remplacer) sur les onglets Phrases / Gages.
4. Ajuster rates et n° de gages, générer les grilles, sync téléphone si besoin.

## Workflow IA (MCP desktop)

1. Lancer Open Bingo PC, activer **MCP IA**, copier le token Bearer.
2. Brancher Cursor (voir [README](../../README.md#mcp-ia-desktop) ou [`.cursor/mcp.json.example`](../../.cursor/mcp.json.example)).
3. Prompt type : *lis `film.sdh.srt`, propose ~80 phrases courtes, crée 5 gages, `add_cases` / `add_gages`.*
4. Affiner dans l’UI, régénérer les grilles.

Via CSV / MCP seuls : l’app **ne parse pas** le fichier SDH — l’IA (ou vous) le lit ; le CSV / MCP ne transporte que phrases et gages.

## Workflow OpenSubtitles (desktop)

Sans fichier SRT local : l’app cherche et télécharge via OpenSubtitles (PC uniquement).

1. Clé API gratuite (opensubtitles.com → API consumers) → **Réglages** / carte OpenSubtitles (locale, jamais commitée ni sync).
2. Page **Bingo film** → titre du film, langue au choix.
3. Résultats avec filtre SDH `hearing_impaired=include` (toutes les pistes, badge SDH, HI en avant) → choisir une piste → cues / phrases.
4. Sélectionner / ajouter au projet, ajuster rates & gages, générer les grilles ; sync téléphone si besoin.

Plan : [`docs/PLAN-opensubtitles.md`](../PLAN-opensubtitles.md).
