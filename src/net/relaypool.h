#pragma once

#include "relayclient.h"
#include "nostr.h"

#include <QObject>
#include <QUrl>
#include <QSet>
#include <QHash>
#include <QStringList>
#include <vector>
#include <memory>
#include <cstdint>

namespace net {

// Manages a pool of RelayClient connections (one per relay URL).
//
// - publishToAll: sends to every connected relay.
// - subscribeAll: souscrit un canal (#t) sur tous les relais ; les
//   souscriptions de plusieurs canaux/listes coexistent sur une même socket
//   et sont ré-appliquées après reconnexion (contrat réseau Colo v1).
// - Deduplication: each event id is tracked; eventReceived is emitted at most once.
// - online property: true iff at least one relay is connected; emits onlineChanged.
class RelayPool : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool online READ isOnline NOTIFY onlineChanged)

public:
    explicit RelayPool(QObject* parent = nullptr);
    ~RelayPool() override;

    // Replace the relay list and reconnect everything.
    void setRelays(const QList<QUrl>& urls);

    // Default relay set (protocole Colo v1 §4 : relais colo-apps).
    static QList<QUrl> defaultRelays();

    // Connect all relays.
    void connectAll();

    // Disconnect all relays.
    void disconnectAll();

    // Arrêt définitif (destruction) : coupe signaux + sockets avant de libérer.
    void shutdown();

    // Publish to every connected relay. Returns how many sockets accepted the send.
    int publishToAll(const NostrEvent& ev);

    // Nombre de relais actuellement connectés.
    int connectedCount() const;

    // Coupe tout et reconnecte (connexions WebSocket « zombies » Android).
    // Ignore les appels trop rapprochés (cooldown) pour éviter le thrashing offline.
    void forceReconnect();
    bool forceReconnectAllowed() const;

    // Souscrit un canal sur tous les relais (une souscription par canal).
    void subscribeAll(const QString& channelTag, int64_t since);
    void unsubscribe(const QString& channelTag);

    bool isOnline() const { return m_online; }

signals:
    // Emitted once per unique event id.
    void eventReceived(const NostrEvent& ev);
    void eose();
    void onlineChanged(bool online);
    void publishAck(const QString& eventId, bool accepted, const QString& msg);

private slots:
    void onClientConnected();
    void onClientDisconnected();
    void onClientEvent(const NostrEvent& ev);
    void onClientEose();
    void onClientAck(const QString& eventId, bool accepted, const QString& msg);

private:
    void updateOnlineState();

    std::vector<std::unique_ptr<RelayClient>> m_clients;
    QSet<QString> m_seenIds;   // in-memory dedup by event id

    // Souscriptions actives par canal (#t) — une entrée par liste partagée.
    QHash<QString, int64_t> m_subscriptions;

    bool m_online = false;
    bool m_shuttingDown = false;
    qint64 m_lastForceReconnectMs = 0;
};

} // namespace net
