# Protocole Open Bingo

Référence normative du protocole de synchronisation d'**Open Bingo**.
Version **1** — dernière mise à jour : **2026-11-12**.

> Ce document remplace l'ancienne `docs/SPEC.md`. Il décrit le produit actuel :
> **relais Nostr chiffré uniquement, sans P2P, sans TURN, sans WebRTC, sans
> notifications push.** La couche réseau reprend le protocole v1 de la famille
> Colo (voir `Colo_Course/PROTOCOL.md`) ; seuls le modèle de données et le
> schéma d'appairage sont propres à Bingo.

## 1. Principes

- Chaque appareil garde une **copie locale complète** (SQLite).
- Un projet partagé = une **clé symétrique 32 octets** = un canal chiffré.
- Le serveur est un **relais Nostr** store-and-forward : il ne peut pas lire les
  projets, seulement stocker et rediffuser des événements chiffrés.
- La synchronisation est **LWW sur le document projet** (`updatedAt`) : un
  événement transporte l'état complet du projet + les coches Play. Il n'y a pas
  de CRDT article-par-article dans Bingo.
- Aucun compte : seule l'installation (`deviceId`) est identifiée.
- Aucune notification push : la sync a lieu **app ouverte** ou au retour au
  premier plan.

## 2. Identifiants

| Nom | Format | Rôle |
|---|---|---|
| `projectId` | chaîne (`proj_…`) | identifiant de projet |
| `deviceId` | UUID v4 | installation, généré une fois |
| `projectKey` | 32 octets | clé symétrique du projet partagé |
| `channelTag` | 32 hex | tag public du canal Nostr |

### Dérivations

```text
channelTag = hex(SHA256("open-bingo/v1/channel" || projectKey))[0..31]
nostrSeed  = SHA256("open-bingo/v1/nostrkey" || projectKey)
```

La clé Nostr de publication est un keypair secp256k1 dérivé de `projectKey` :
tous les participants d'un projet partagent la même identité de publication, les
relais ne distinguent pas les appareils.

> Le préfixe `open-bingo/v1` est conservé pour rester compatible avec les projets
> déjà partagés par les versions v2.x. Ne jamais le changer sans migration.

## 3. Modèle de données

- **Project** : `id`, `title`, `description`, `gridSize`, `players`, `cases`,
  `gages`, `grids`, `gageMode`, `comboGages`, `multipliers`, `createdAt`,
  `updatedAt`.
- **Coches Play** : état `players × grille` (booléens), stocké à part du projet.
- **Overlays Play** : file one-shot (gages déclenchés, gagnant + classement) à
  coller dans le prochain snapshot ; elle n'est pas rejouée sur un echo local.
- **Merge** : le snapshot reçu est adopté si
  `remote.updatedAt >= local.updatedAt` (ou si le local est vide/stub) ; les
  coches Play sont adoptées avec le même snapshot.

`updatedAt` est mis à jour à chaque modification locale publiée ; c'est
l'horodatage qui arbitre le LWW.

## 4. Transport Nostr

- Relais par défaut : `wss://colo-apps.les-crevettes-cevenoles.fr`.
- Relais configurable dans Réglages (une URL `wss://…` par ligne) ; publication
  sur tous les relais connectés, réception dédupliquée par id d'événement.
- Événement Nostr : `kind = 4545`, tag `["t", channelTag]`.
- Souscription : **une souscription par projet partagé**, sur la même socket :

```json
{"kinds":[4545],"#t":[channelTag],"since":0}
```

- Contenu : `base64(nonce || ciphertext)`.
- Chiffrement : **XChaCha20-Poly1305** (libsodium), nonce aléatoire 24 octets,
  AD = `channelTag`.
- Le clair est un JSON compressé (`z:` deflate-raw, ou `b64:` en repli) puis
  chiffré : les grilles × joueurs font sinon exploser la taille des événements.
- Taille visée : **< ~120 Ko** par événement (limite de frame du relais).
  Si un snapshot dépasse, réduire le nombre de joueurs/grilles ou scinder.

## 5. Payload

Le clair décompressé est un `ProjectBundle` JSON :

```json
{
  "project": {
    "id": "proj_…",
    "title": "Bingo soirée",
    "updatedAt": 1763000000000,
    "gridSize": 5,
    "players": [{"name": "Alice"}],
    "cases": [{"label": "Cliché", "points": 2, "rate": 80}],
    "grids": [{"player": "Alice", "cells": [[ ... ]]}],
    "gages": [{"description": "…", "hp": 1, "number": 3, "rate": 100}],
    "gageMode": false,
    "comboGages": {"line": "", "column": "", "diagonal": ""},
    "multipliers": {"line": 2, "column": 2, "diagonal": 3, "full": 10}
  },
  "playChecks": { "Alice": [[false, true, …]] },
  "playOverlays": [ { "kind": "gage", "…": "…" } ]
}
```

- `playChecks` / `playOverlays` sont optionnels (les anciens snapshots n'en ont
  pas : dans ce cas les coches locales ne sont pas écrasées).
- Champs inconnus : ignorés (forward-compat).
- Un snapshot est **auto-suffisant** : il remplace l'état local s'il est plus
  récent. Aucun delta incrémental.

### File d'attente et accusés

- Chaque snapshot publié est d'abord rangé dans l'`outbox` (un seul en attente
  par projet) ; il n'est retiré que sur **accusé durable du relais**
  (`OK true`, ou `duplicate` / `already` / `stored`).
- Un rejet transitoire n'est pas fatal : un autre relais peut accepter. Un rejet
  définitif (trop gros, tags invalides) reconstruit un snapshot frais.
- Les ids d'événements déjà traités sont mémorisés (`seen_events`) : un rejeu
  après reconnexion ou multi-relais est ignoré.
- À la reconnexion : flush de l'outbox puis re-souscription de tous les canaux.
- **Rehost** : une fois par projet et par installation (`rehostV2.<projectId>`),
  le client republie un snapshot complet — utile quand le relais a été remplacé
  ou vidé. Il suffit qu'un appareil possédant le projet soit en ligne.

## 6. Appairage

```text
openbingo://join/1/<projectId>/<base64url(projectKey)>/<urlencode(titre)>
```

- QR code ou lien (WhatsApp, SMS…). Sur Android, scan caméra ou collage ;
  sur desktop, collage du lien.
- Le lien contient la clé : **ne pas le publier**.
- Le nouvel appareil crée un projet local vide (stub `updatedAt = 0`), souscrit
  au canal et publie le contenu dès qu'il le reçoit / dès que l'hôte repartage.
- Révocation non supportée : pour exclure quelqu'un, recréer un projet avec une
  nouvelle clé et repartager.

## 7. Notifications

**Aucune.** Open Bingo n'utilise ni FCM, ni UnifiedPush, ni ntfy, ni le broker
`colopush`. Conséquences assumées :

- la sync se fait app ouverte / au premier plan ;
- aucun réveil en arrière-plan, aucune notification système de changement
  (seule une notification locale « gagnant » peut être affichée pendant une
  partie, comme n'importe quelle alerte de jeu).

## 8. Comportement attendu

1. Modification locale → sauvegarde SQLite → snapshot chiffré → outbox → relais.
2. Réception → déchiffrement → merge LWW → mise à jour locale → UI.
3. Retour réseau / premier plan → flush outbox → re-souscription → rehost.
4. App en arrière-plan : plus de sync (reprise à l'ouverture).
5. App tuée : l'ouverture rattrape l'état du relais.

## 9. Sécurité et limites

- Le relais voit : `projectId`, `channelTag`, tailles, horodatages, adresses IP.
- Il ne voit pas : titre, phrases, grilles, joueurs, coches.
- Quiconque possède le lien (donc la clé) est membre à vie du projet.
- Perte possible si tous les appareils disparaissent : le relais ne conserve pas
  éternellement les snapshots.
- Pas de photos, pas de HD : un projet bingo tient dans un snapshot JSON.
- Le protocole est **v1** : champs inconnus ignorés, pas de version `v` dans le
  payload (contrairement à Colo) — la compatibilité est assurée par le schéma
  `ProjectBundle`.
