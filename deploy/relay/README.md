# Relais Nostr — colo-apps (partagé avec Colo Course)

Open Bingo utilise le **même serveur** que Colo Course :

| Élément | Valeur |
|---|---|
| **URL relais** | `wss://colo-apps.les-crevettes-cevenoles.fr` |
| **URL push ntfy** | `https://colo-apps.les-crevettes-cevenoles.fr/ntfy` |
| **Topics push** | `bingo-{channelTag}` (un topic par projet partagé) |
| **Backend relais** | Strfry Docker → `127.0.0.1:7777` |
| **Backend ntfy** | ntfy Docker → `127.0.0.1:8077` |
| **TLS nginx** | SNI `:443` → `127.0.0.1:11443` |

## Sur le serveur (déjà en place)

Le relais Strfry et ntfy tournent pour Colo Course — **aucune instance supplémentaire**
n'est nécessaire pour Open Bingo. Les deux apps publient des events Nostr kind 4545
sur des canaux chiffrés distincts ; les topics ntfy sont préfixés `bingo-` vs `colo-`.

```bash
# Relais (une seule fois sur le serveur) :
cd ~/colocourse-relay && docker compose up -d

# ntfy (une seule fois) :
cd ~/colocourse-ntfy && docker compose up -d
```

Fichiers de référence versionnés : `deploy/relay/docker-compose.yml`, `deploy/ntfy/docker-compose.yml`
