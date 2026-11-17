# Relais de synchronisation — serveur partagé colo-apps

Open Bingo n'a **pas de serveur dédié** : il utilise le même relais chiffré que
Colo Course et Colo Tâches, sur `wss://colo-apps.les-crevettes-cevenoles.fr`.

| Élément | Valeur |
|---|---|
| Relais | Strfry (Nostr), kind `4545` uniquement |
| URL | `wss://colo-apps.les-crevettes-cevenoles.fr` |
| Écriture | `write-policy.py` : seul le kind 4545 est accepté |
| Taille max | 256 Ko (`maxEventSize` + `maxWebsocketFrameSize`) |
| Chiffrement | XChaCha20-Poly1305, clé par projet — le relais ne lit rien |

## Notifications

Open Bingo **n'utilise pas** le broker push (`colopush` / FCM / UnifiedPush / ntfy).
La synchronisation se fait quand l'app est ouverte ou au retour au premier plan ;
aucun topic `bingo-*`, aucun endpoint push à enregistrer.

## Déploiement

Le relais est celui de la famille Colo. Les fichiers de référence (compose,
`strfry.conf`, `write-policy.py`, nginx, broker) sont maintenus dans
[Colo_Course/deploy](https://github.com/Poisson48/Colo_Course/tree/main/deploy)
et documentés dans [docs/SYNC-SERVER.md](../docs/SYNC-SERVER.md) ;
voir aussi [PROTOCOL.md](../PROTOCOL.md) pour le format des événements.

## Vérifications utiles

```bash
curl -fsS https://colo-apps.les-crevettes-cevenoles.fr/         # relais vivant
docker compose ps                                              # sur le serveur
```

Symptômes :

- modifications locales jamais reçues ailleurs → write-policy, taille max, ou
  abonnement (canal) ;
- `event too large` → augmenter `maxEventSize` **et** `maxWebsocketFrameSize` ;
- relais remplacé/vidé → les clients re-sèment leur projet une fois
  (`rehostV2.<projectId>`), il suffit qu'un appareil soit en ligne.
