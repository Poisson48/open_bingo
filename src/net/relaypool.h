#pragma once

#include "relayclient.h"
#include "nostr.h"

#include <QObject>
#include <QUrl>
#include <QSet>
#include <QStringList>
#include <vector>
#include <memory>
#include <cstdint>

namespace net {

// Manages a pool of RelayClient connections (one per relay URL).
//
// - publishToAll: sends to every connected relay.
// - subscribeAll: subscribes all relays (and re-subscribes on reconnect via
//   RelayClient's built-in mechanism).
// - Deduplication: each event id is tracked; eventReceived is emitted at most once.
// - online property: true iff at least one relay is connected; emits onlineChanged.
class RelayPool : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool online READ isOnline NOTIFY onlineChanged)

public:
    explicit RelayPool(QObject* parent = nullptr);
    ~RelayPool() override = default;

    // Replace the relay list and reconnect everything.
    void setRelays(const QList<QUrl>& urls);

    // Default relay set from SPEC §3.1.
    static QList<QUrl> defaultRelays();

    // Connect all relays.
    void connectAll();

    // Disconnect all relays.
    void disconnectAll();

    // Publish to every connected relay. Returns how many sockets accepted the send.
    int publishToAll(const NostrEvent& ev);

    // Nombre de relais actuellement connectés.
    int connectedCount() const;

    // Coupe tout et reconnecte (connexions WebSocket « zombies » Android).
    // Ignore les appels trop rapprochés (cooldown) pour éviter le thrashing offline.
    void forceReconnect();
    bool forceReconnectAllowed() const;

    // Subscribe on every relay (accumulates channel tags — un filtre #t multi-valeurs).
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

    // Active subscription tags (OR on #t) — re-applied for new relays / reconnect.
    QStringList m_channelTags;
    int64_t     m_since = 0;

    bool m_online = false;
    qint64 m_lastForceReconnectMs = 0;
};

} // namespace net
