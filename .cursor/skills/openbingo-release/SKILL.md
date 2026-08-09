---
name: openbingo-release
description: >-
  Publie une release Open Bingo (bump version, README, commit, tag annoté v*,
  push GitHub Actions). Use when the user asks to release, publier, ship,
  tagger, sortir une version, bump, or says go / ok for a release.
---

# Open Bingo — Release

Appliquer **systématiquement** ce skill dès qu’une release est demandée (ou un « go » après un fix destiné à sortir). Ne pas inventer d’autre procédure.

## Prérequis

- Branche `main` (ou confirmation explicite de l’utilisateur sinon).
- Changements prêts : build local + tests verts pour le périmètre touché.
- Ne **pas** committer : `.tmp/`, `build*/`, `*.apk`, `aqtinstall.log`, keystores.
- Ne **pas** forcer-push ni supprimer un tag distant sans demande explicite.

## Checklist

```
Release Progress:
- [ ] 1. Déterminer la version
- [ ] 2. Bump fichiers version
- [ ] 3. Mettre à jour le README (obligatoire)
- [ ] 4. Build + ctest
- [ ] 5. Commit
- [ ] 6. Tag annoté
- [ ] 7. Push main + tag
- [ ] 8. Vérifier le workflow Release
```

## 1. Déterminer la version

```bash
git fetch --tags origin
git tag -l 'v*' --sort=-v:refname | head -5
```

- Patch `vX.Y.(Z+1)` pour correctifs / petites UX.
- Minor si fonctionnalité visible (rare ici : on reste en 2.0.x patch).
- Tag = `v` + `versionName` (ex. `2.0.40` → `v2.0.40`).
- Si le bump est déjà dans le working tree, réutiliser cette version.

## 2. Bump fichiers version

Aligner **au minimum** :

| Fichier | Champ |
|---|---|
| `CMakeLists.txt` (racine) | `project(openbingo VERSION X.Y.Z …)` |
| `android/AndroidManifest.xml` | `android:versionName` **et** `android:versionCode` |

`versionCode` : entier monotone (en pratique le patch de `2.0.N` → `N`, cohérent avec l’historique local). La CI recalcule aussi un code via `git rev-list --count` pour les artefacts GitHub ; le manifest local doit quand même monter.

Ne pas se fier à `src/CMakeLists.txt` `BINGO_VERSION_*` (defaults CACHE) : la release CI passe `-DBINGO_VERSION_*`.

## 3. Mettre à jour le README (obligatoire)

**Toujours** relire et mettre à jour `README.md` avant le tag — même pour un hotfix.

Au minimum :

1. Section **Fonctionnalités** : refléter les nouveautés / comportements visibles (ajout, reformulation, ou retrait si obsolete).
2. Cohérence des mentions de version / compatibilité (bloc `v2.x`, notes Android vs v1, etc.).
3. Si la release change le flux utilisateur documenté (**Comment ça marche**, téléchargements, build, **Publier une release**), aligner le texte.
4. Captures : si l’UI a beaucoup changé et que les screenshots trompent, lancer `bash scripts/capture-screenshots.sh` (ou noter clairement qu’il faudra le faire) et committer `docs/assets/screenshots/` si générés.

Optionnel si le site public reprend les mêmes points : aligner `docs/index.html` (section fonctionnalités / texte marketing).

Ne pas inventer de changelog long dans le README : le message du **tag** est le changelog utilisateur.

## 4. Build + tests

```bash
cmake -S . -B build -G Ninja
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

Si seuls certains modules ont changé, au minimum les `tst_*` concernés + un rebuild propre. Ne pas tagger sur tests rouges.

## 5. Commit

Messages en français, conventional commits (`fix:`, `feat:`, `chore:`, …). Inclure le bump version **et** le README dans le même commit release (ou commit feature puis commit `chore: release 2.0.N` si l’historique le demande — préférer un commit clair).

Suivre les règles git du dépôt (HEREDOC, pas de `--no-verify`, pas d’amend sauf conditions habituelles).

## 6. Tag annoté

Le corps du tag = notes affichées dans l’app (Updater) **et** sur GitHub Releases.

```bash
git tag -a "vX.Y.Z" -m "$(cat <<'EOF'
Résumé court en français (1–3 phrases, orienté utilisateur).

Mettez à jour les deux appareils.
EOF
)"
```

- Toujours terminer par **`Mettez à jour les deux appareils.`** quand la sync / le play multi-appareils est concerné (défaut recommandé pour toute release 2.0).
- Tag **annoté** (`-a`) obligatoire — un tag léger casse les notes CI.
- Ne pas réutiliser un tag existant.

## 7. Push

```bash
git push origin HEAD
git push origin "vX.Y.Z"
```

Le push du tag `v*` déclenche `.github/workflows/release.yml` → APK arm64 + AppImage + GitHub Release.

## 8. Vérifier la CI

```bash
gh run list --workflow=release.yml --limit 3
gh release view "vX.Y.Z"
```

Donner à l’utilisateur : URL du run Actions + URL `https://github.com/Poisson48/open_bingo/releases/tag/vX.Y.Z`.

Si le workflow échoue : diagnostiquer, corriger sur `main`, **nouveau** patch (`vX.Y.Z+1`) — ne pas retagger le même nom sauf demande explicite.

## Contre-indications

- Pas de release sans mise à jour README (étape 3).
- Pas de release « silencieuse » sans tag annoté compréhensible.
- Pas d’inclusion d’artefacts locaux (`*.apk`, builds) dans le commit.
- Attendre le feu vert utilisateur si la demande est ambiguë (« peut‑être qu’on sortira ») ; « go », « release », « publie », « tague » = exécuter.
