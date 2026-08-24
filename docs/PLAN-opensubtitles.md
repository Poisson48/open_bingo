# Plan — OpenSubtitles → bingo film (SDH)

> Suite de [`PLAN-csv-mcp.md`](PLAN-csv-mcp.md).  
> **Ne pas coder avant validation** des décisions en fin de doc.  
> Méthode : multi-agents + **toujours** un agent **UI/UX** dédié et un agent **garde-fou**.

## Objectif produit

L’utilisateur tape le **nom du film** dans Open Bingo → l’app interroge **OpenSubtitles.com** → choisit une piste **SDH / malentendants** (hearing impaired) → télécharge le `.srt` → en extrait des **cues** propres → propose / ajoute des **phrases bingo** (manuel, sélection, ou MCP/IA).

But : bingo film **sans avoir vu le film**, sans chasser un fichier SRT à la main, avec une **grosse passe UI/UX** (pas un coin de Réglages).

## Parcours cible

```
Nom du film (+ année / langue)
        │
        ▼
Open Bingo UI ──HTTPS──► api.opensubtitles.com
        │                      │
        │                      ├── GET /subtitles?query=…&hearing_impaired=include|only
        │                      └── POST /download { file_id } → lien temporaire
        ▼
Fichier .srt (cache local)
        │
        ├── Preview cues (timestamps + texte nettoyé)
        ├── Sélection → add_cases / CSV
        └── MCP : resource / tool pour l’IA (Cursor)
```

Exemple :

> Projet « Bingo Matrix ». Cherche « The Matrix 1999 », langue FR, **SDH only**. Choisis la piste la plus téléchargée. Propose 60 répliques courtes (ignore `[bruitages]`). Ajoute-les en phrases.

## État actuel (déjà livré sur `feat/csv-mcp`)

| Brique | Statut |
|---|---|
| CSV phrases/gages + UI | ✅ |
| MCP HTTP localhost | ✅ |
| Parse SRT dans l’app | ❌ |
| Client OpenSubtitles | ❌ |
| UI « bingo film » / recherche | ❌ |
| Tools MCP sous-titres | ❌ |

---

## Contraintes OpenSubtitles (à respecter)

API officielle : `https://api.opensubtitles.com/api/v1`  
Docs : [opensubtitles.stoplight.io](https://opensubtitles.stoplight.io/)

| Règle | Détail |
|---|---|
| **Api-Key** | Header obligatoire sur chaque requête (clé **consommateur** créée sur le compte OS) |
| **User-Agent** | Obligatoire, forme `OpenBingo v2.x.y` (version app) |
| **Login JWT** | Optionnel : `POST /login` → Bearer pour quotas download plus élevés |
| **Search** | `GET /subtitles?query=…&languages=fr&type=movie&hearing_impaired=only\|include\|exclude` |
| **Download** | `POST /download` body `{ "file_id": N }` → URL temporaire + quota |
| **Quotas** | Anonyme très bas ; compte free ~10 DL/jour ; VIP bien plus — **afficher le restant** |
| **Pas de clé partagée dans le repo** | Open source → chaque utilisateur (ou le mainteneur en build privé) fournit sa clé |

### Auth produit (figé)

1. **Clé API utilisateur saisie sur PC** (SQLite `settings`, jamais sync Nostr / jamais git).
2. **Login free** username/password → JWT (quotas ~20 DL/j) — pas besoin de VIP pour un usage bingo.
3. Lien d’aide : « Créer une clé + compte free sur opensubtitles.com ».
4. Pas de scraping massif ; provider `.org` web = fallback best-effort seulement.
5. Feature OpenSubtitles **desktop-only** en v1.

### Filtre SDH / langue (figé)

- `hearing_impaired=include` : toutes les pistes, **badge SDH**, tri préférant HI.
- Langue : **sélecteur libre** (mémoriser le dernier choix) — pas de langue imposée.
- Afficher `hearing_impaired`, `language`, `download_count`, `fps`, `release`, année, feature title.

---

## Architecture technique

```
src/
  core/
    srtcodec.h/.cpp      # parse SRT/VTT → vector<Cue> ; strip tags SDH optionnel
    phraseextract.h/.cpp # heuristiques : dédup, ignore [bruit], longueur, scoring
  net/  (ou app/)
    opensubtitlesclient.h/.cpp  # Qt Network : search, login, download
  app/
    filmassistant.h/.cpp # état UI : query, results, selected, cues, busy/error
    appcontroller        # pont Q_INVOKABLE + settings key
    mcpserver            # tools : search_subtitles, download_subtitle, preview_cues…
  qml/
    FilmAssistPage.qml   # expérience principale (grosse UX)
    FilmSearchBar.qml
    SubtitleResultDelegate.qml
    CueList.qml
```

### Modèle `Cue`

```cpp
struct Cue {
    int startMs = 0;
    int endMs = 0;
    std::string text;       // brut (peut contenir <i>, {\an8}, etc.)
    std::string plain;      // nettoyé pour bingo
    bool likelySfx = false; // [bruit], (musique), etc.
};
```

### Settings (SQLite `settings`)

| Clé | Valeur |
|---|---|
| `os_api_key` | string |
| `os_username` | string (opt) |
| `os_jwt` / `os_jwt_exp` | token cache (opt) |
| `os_default_lang` | `fr` / `en` / … |
| `os_prefer_hi` | `0`/`1`/`2` (exclude/include/only) |

Cache fichiers : `AppDataLocation/subtitles/<feature_id>_<file_id>.srt` (TTL ou manuel « vider cache »).

### MCP (extensions)

| Tool | Rôle |
|---|---|
| `search_subtitles` | query, lang, hearing_impaired, type |
| `download_subtitle` | file_id → path local + stats |
| `preview_cues` | path ou file_id déjà DL, max N, skip_sfx |
| `suggest_bingo_phrases` | heuristique locale (pas d’IA cloud) → liste labels |
| `import_cues_as_cases` | indices ou textes → `add_cases` |

L’IA Cursor peut aussi `Read` le `.srt` local après download — le tool `preview_cues` réduit les tokens.

---

## UI / UX — passe majeure (priorité)

**Pas** un bouton caché. Expérience **« Bingo film »** de premier niveau.

### Principes

1. **Une composition** : écran dédié (pas un dashboard de settings).
2. **Un job par étape** : Chercher → Choisir → Prévisualiser → Ajouter.
3. **Brand Open Bingo** : surfaces Theme existantes (`surface`, accent indigo), pas un look « API admin ».
4. **SDH visible** : badge « Malentendants », toggle clair, copy qui explique pourquoi.
5. **États** : idle / loading skeleton / empty / erreur quota / succès.
6. **Desktop d’abord** (réseau + MCP) ; Android : même flow si clé OK, sans MCP.
7. **Motion légère** : fade résultats, highlight sélection, progress download (2–3 motions max).

### Entrées UX

| Entrée | Où |
|---|---|
| CTA **« Depuis un film… »** | haut de **Phrases** (`CasesPage`) |
| Entrée menu projet | Editor overflow |
| Raccourci Réglages | section OpenSubtitles (clé API seulement) — pas le flow complet |

### Wireframe `FilmAssistPage`

```
┌─────────────────────────────────────────────┐
│  ← Phrases          Bingo film              │
│  Prépare des phrases depuis les sous-titres │
│  malentendants — sans regarder le film.     │
│                                             │
│  ┌─────────────────────────────────────┐    │
│  │  Titre du film…            [Chercher]│    │
│  └─────────────────────────────────────┘    │
│  Langue [au choix ▾]   SDH mis en avant (include)     │
│                                             │
│  Résultats                                  │
│  ┌─────────────────────────────────────┐    │
│  │ ★ The Matrix (1999) · FR · SDH      │    │
│  │   12k DL · BluRay · fps 23.976      │    │
│  └─────────────────────────────────────┘    │
│  ┌─────────────────────────────────────┐    │
│  │   The Matrix (1999) · FR            │    │
│  │   8k DL · DVD                       │    │
│  └─────────────────────────────────────┘    │
│                                             │
│  Aperçu (après choix)                       │
│  ☑ Ignore bruitages   ☑ Phrases ≤ 60 car.   │
│  ☐ 00:12:03  On ne parle pas de Fight…      │
│  ☐ 00:14:22  Je te vois…                    │
│  …                                          │
│  [Tout cocher utiles]  [Ajouter N phrases]  │
│  [Exporter CSV]  [Ouvrir pour l’IA / MCP]    │
└─────────────────────────────────────────────┘
```

### Copy (FR)

- Titre page : **Bingo film**
- Sous-titre : *Sous-titres malentendants → phrases bingo, sans spoiler visuel.*
- Erreur sans clé : *Ajoute ta clé OpenSubtitles (gratuite) dans Réglages pour chercher des films.*
- Quota : *Plus de téléchargements aujourd’hui (OpenSubtitles). Réessaie demain ou connecte un compte.*
- Attribution discrète : *Sous-titres via OpenSubtitles.com* (lien).

### Réglages (minimal)

Carte **OpenSubtitles** sous MCP :

- Champ clé API (masqué, coller, tester connexion)
- Login optionnel
- Langue par défaut
- Préférence SDH
- Lien doc + « Vider le cache sous-titres »

### Accessibilité / mobile

- Targets ≥ `Theme.touchTarget`
- Résultats scrollables, pas de cards inutiles hors interaction
- Sur petit écran : étapes en StackView (Search → Results → Cues)

---

## Phases / sprints

| Sprint | Livrable | Done when |
|---|---|---|
| **0** | Contrats : `srtcodec`, `OpenSubtitlesClient`, API `FilmAssistant`, tools MCP list, **maquettes UX validées** | Headers + page QML squelette + plan figé |
| **1** | Client API + parse SRT + tests (fixtures SRT, mock HTTP) | Search/download against **vrai** OpenSubtitles (clé dev) + 1 film connu |
| **2** | **UI/UX complète** `FilmAssistPage` + Réglages clé | Flow bout-en-bout sans MCP |
| **3** | Heuristique suggest phrases + import cases/CSV + MCP tools | Cursor peut `search_subtitles` → `add_cases` |
| **4** | Polish : quotas UI, cache, tray, doc README, skill film | Soirée bingo prêt |

Ordre : **0 → 1 → 2** (UX bloquante pour le feeling) ; 3 peut chevaucher fin de 2 via agents.

### Test réel OpenSubtitles (obligatoire avant merge)

1. Clé API de test (secret local / env `OPENSUBTITLES_API_KEY`, **jamais commit**).
2. Chercher un film libre de droits / classique abondant (ex. titre public domain ou blockbuster très fourni) en **FR + SDH**.
3. Download → parse → ≥ N cues.
4. Smoke manuel UI + test d’intégration **skippable** si pas de clé (`ctest -L opensubtitles` ou env manquante → skip).

Fixture : un `.srt` SDH anonymisé dans `tests/fixtures/` pour les tests offline (pas de dépendance réseau en CI).

---

## Méthode agents

Toujours :

| Rôle | Mission |
|---|---|
| **UI/UX (dédié)** | `FilmAssistPage` + composants + copy + états ; seul sur QML film |
| **Garde-fou (dédié)** | contrats, fixtures SRT, skip CI, pas de clé en repo, quotas/erreurs |

Fan-out typique Sprint 1–3 :

| Agent | Scope |
|---|---|
| **OS client** | `opensubtitlesclient.*` |
| **SRT codec** | `srtcodec.*` + fixtures |
| **FilmAssistant / controller** | état + settings + import cues |
| **MCP tools** | search/download/preview seulement |
| **Doc** | README + exemples + ToS note |
| **UI/UX** | page + navigation |
| **Garde-fou** | tests + revue sécu clé |

1 lot = 1 famille de fichiers. Contrats Sprint 0 avant fan-out.

---

## Hors scope (volontaire)

- Scraping opensubtitles.com HTML
- Transcription audio / OCR du film
- Traduction IA cloud des sous-titres
- Clé API Open Bingo partagée publique
- Sync de la clé API via Nostr
- Matching par hash fichier vidéo (v2 possible)

## Risques & mitigations

| Risque | Mitigation |
|---|---|
| Quota / 429 | Afficher remaining + Retry-After ; cache SRT |
| Mauvaise piste (fan / spoil tags) | Badge HI, tri download_count, preview avant import |
| Clé commitée | `.gitignore`, settings only, test skip CI |
| API change / DMCA titles | Erreurs claires ; ne pas hardcoder un film blacklisté en smoke |
| Phrases trop longues / bruitages | Heuristique `likelySfx` + filtre longueur UI |
| UX « formulaire API » | Agent UI/UX + revue avant merge Sprint 2 |

---

## Décisions figées

| # | Choix |
|---|---|
| 1 | **Clé API saisie sur PC** (Réglages / carte OpenSubtitles) — stockée locale, jamais Nostr |
| 2 | **Login free OS** username/password en v1 (JWT) — pour ~20 DL/jour **gratuits** (pas VIP) |
| 3 | SDH : **`hearing_impaired=include`** — toutes les pistes, **badge SDH** + tri qui met HI en avant |
| 4 | **Langue au choix** (sélecteur UI ; mémoriser le dernier choix) |
| 5 | Navigation **StackView** page dédiée « Bingo film » |
| 6 | OpenSubtitles **desktop-only v1** ; Android reste sync / jeu |
| 7 | **Test live** parcimonieux (search OK ; **≤1–2 downloads**/session de test) |
| 8 | **Deux sources** : `.com` API (officiel) + fallback **`.org` web** (best-effort) — voir § Providers |

### Sécu clé API

- **Ne jamais commit** la clé (ni dans README, tests, agents prompts loggés).
- Si une clé a fuité dans un chat / ticket : **la régénérer** sur opensubtitles.com → API consumers.
- Fichiers locaux autorisés (gitignore) : `.secrets/opensubtitles.env`  
  `OPENSUBTITLES_API_KEY=…` / `OPENSUBTITLES_USERNAME=…` / `OPENSUBTITLES_PASSWORD=…`

### Providers (figé après feedback « .com payant »)

Clarification : l’API **opensubtitles.com est gratuite** avec quotas (sans compte ~5 DL/j ; **compte free + login ~20 DL/j**). Le payant = VIP / pro pour plus de volume.

| Provider | Méthode | Usage |
|---|---|---|
| **A — `.com` REST** | Api-Key + login JWT free | **Principal** — search + download officiels |
| **B — `.org` web** | Fetch HTML search + lien download (best-effort) | **Fallback** si quota `.com` ou échec ; fragile |

L’ancienne XML-RPC `api.opensubtitles.org` est **éteinte** — on ne s’appuie pas dessus.

**Provider B (scraping `.org`)** — contraintes :
- Uniquement desktop, User-Agent `OpenBingo/…`, rate-limit strict (ex. ≥1,5 s entre requêtes).
- Parse ciblé pages `/search` (titre + langue), pas de crawl massif.
- Échec silencieux → message « Source .org indisponible, utilise .com / login ».
- ToS / fragilité UI : feature **best-effort**, pas garantie CI.
- Préférer toujours **search** ; download `.org` seulement si l’utilisateur clique (même parcimonie).

UI Réglages : sélecteur **Source** = `Auto` (`.com` puis `.org`) | `.com` only | `.org` only.

---

## Lien plan global

| Phase | Contenu |
|---|---|
| 6 | CSV + MCP desktop (en cours / livré sur branche) |
| **7** | **OpenSubtitles + Bingo film UI + SRT + MCP tools** |

Fan-out agents en cours ; **quota download** : tests live = search d’abord, DL rare.
