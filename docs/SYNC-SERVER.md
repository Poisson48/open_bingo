# Serveur de synchronisation

Dernière mise à jour : **2026-11-12**. Open Bingo utilise le **même serveur que
les Colo** : un relais Nostr chiffré + (pour les Colo uniquement) un broker de
notifications. **Pas de P2P, pas de TURN, pas de WebRTC.**

```text
Appareil A ── wss ──► Strfry (relais Nostr) ── wss ──► Appareil B
```

| Composant | Rôle | Port / URL |
|---|---|---|
| Strfry | relais Nostr kind 4545, stockage des blobs chiffrés | `127.0.0.1:7777`, nginx `:443` `/` |
| nginx | TLS et reverse proxy | `:443` |

Open Bingo **n'utilise pas** `colopush`, FCM, UnifiedPush ni ntfy : aucun topic
push, aucun endpoint enregistré, aucune notification.

## Relais

- `wss://colo-apps.les-crevettes-cevenoles.fr` (configurable dans Réglages) ;
- kind `4545` uniquement, tag `t=channelTag` ;
- blobs chiffrés XChaCha20-Poly1305, clé par projet ;
- `maxEventSize` 256 Ko **et** `maxWebsocketFrameSize` ≥ 262144 (défaut Strfry
  128 Ko — sinon les snaps > ~130 Ko sont refusés) ;
- aucun accès en clair aux projets.

## Rétention et rehost

- le relais ne conserve pas éternellement les snapshots ; chaque modification
  republie l'état complet du projet, donc un appareil en ligne suffit à re-semer ;
- un client qui revient republie une fois chaque projet partagé
  (`rehostV2.<projectId>`) : utile après un changement/raz de relais.

## Déploiement

Le serveur est partagé avec les Colo. Les fichiers de référence (compose,
`strfry.conf`, `write-policy.py`, nginx) sont maintenus dans
[Colo_Course/deploy](https://github.com/Poisson48/Colo_Course/tree/main/deploy)
et documentés dans `Colo_Course/docs/{SYNC-SERVER,RELAY}.md`.

Vérifications :

```bash
curl -fsS https://colo-apps.les-crevettes-cevenoles.fr/       # relais vivant
systemctl is-active nginx
```

Les volumes Strfry sont à sauvegarder. Si tous les appareils disparaissent, un
projet peut être perdu : le relais ne conserve pas éternellement les snapshots.

Voir [PROTOCOL.md](../PROTOCOL.md) pour le format des événements.
