#include "relaypool.h"

#include <QDateTime>
#include <QDebug>

namespace net {

RelayPool::RelayPool(QObject* parent)
    : QObject(parent)
{}

RelayPool::~RelayPool()
{
    shutdown();
}

// static
QList<QUrl> RelayPool::defaultRelays()
{
    // Protocole Colo v1 §4 : le relais colo-apps est le défaut.
    // Relais supplémentaires configurables dans Réglages (une URL par ligne).
    return {
        QUrl(QStringLiteral("wss://colo-apps.les-crevettes-cevenoles.fr")),
    };
}

void RelayPool::setRelays(const QList<QUrl>& urls)
{
    if (m_shuttingDown)
        return;

    // Disconnect old clients.
    for (auto& c : m_clients)
        c->disconnectFromRelay();
    m_clients.clear();

    for (const QUrl& url : urls) {
        // Pas de parent QObject : propriété exclusive via unique_ptr.
        auto client = std::make_unique<RelayClient>(url);

        connect(client.get(), &RelayClient::connected,
                this, &RelayPool::onClientConnected);
        connect(client.get(), &RelayClient::disconnected,
                this, &RelayPool::onClientDisconnected);
        connect(client.get(), &RelayClient::eventReceived,
                this, &RelayPool::onClientEvent);
        connect(client.get(), &RelayClient::eose,
                this, &RelayPool::onClientEose);
        connect(client.get(), &RelayClient::publishAck,
                this, &RelayPool::onClientAck);

        m_clients.push_back(std::move(client));
    }

    // Ré-appliquer toutes les souscriptions actives (multi-listes).
    for (auto it = m_subscriptions.constBegin(); it != m_subscriptions.constEnd(); ++it) {
        for (auto& c : m_clients)
            c->subscribe(it.key(), it.value());
    }
}

void RelayPool::connectAll()
{
    if (m_shuttingDown)
        return;
    for (auto& c : m_clients)
        c->connectToRelay();
}

void RelayPool::disconnectAll()
{
    for (auto& c : m_clients)
        c->disconnectFromRelay();
}

void RelayPool::shutdown()
{
    if (m_shuttingDown)
        return;
    m_shuttingDown = true;
    qDebug() << "[shutdown] RelayPool::shutdown";

    disconnect();

    for (auto& c : m_clients) {
        c->disconnect(this);
        c->shutdown();
    }
    m_clients.clear();
    m_subscriptions.clear();
    m_seenIds.clear();
    m_online = false;
}

int RelayPool::publishToAll(const NostrEvent& ev)
{
    if (m_shuttingDown)
        return 0;
    int sent = 0;
    for (auto& c : m_clients) {
        if (c->isConnected()) {
            c->publish(ev);
            ++sent;
        }
    }
    return sent;
}

int RelayPool::connectedCount() const
{
    int n = 0;
    for (const auto& c : m_clients) {
        if (c->isConnected())
            ++n;
    }
    return n;
}

bool RelayPool::forceReconnectAllowed() const
{
    constexpr qint64 kCooldownMs = 8000;
    if (m_lastForceReconnectMs == 0)
        return true;
    return (QDateTime::currentMSecsSinceEpoch() - m_lastForceReconnectMs) >= kCooldownMs;
}

void RelayPool::forceReconnect()
{
    if (m_shuttingDown)
        return;
    if (!forceReconnectAllowed()) {
        qDebug() << "[RelayPool] forceReconnect skipped (cooldown)";
        return;
    }
    m_lastForceReconnectMs = QDateTime::currentMSecsSinceEpoch();
    for (auto& c : m_clients) {
        c->disconnectFromRelay();
        c->connectToRelay();
    }
}

void RelayPool::subscribeAll(const QString& channelTag, int64_t since)
{
    if (m_shuttingDown || channelTag.isEmpty())
        return;
    m_subscriptions[channelTag] = since;
    for (auto& c : m_clients)
        c->subscribe(channelTag, since);
}

void RelayPool::unsubscribe(const QString& channelTag)
{
    if (m_shuttingDown || channelTag.isEmpty())
        return;
    m_subscriptions.remove(channelTag);
    for (auto& c : m_clients)
        c->unsubscribe(channelTag);
}

// ── Private slots ──────────────────────────────────────────────────────────

void RelayPool::onClientConnected()
{
    if (m_shuttingDown)
        return;
    updateOnlineState();
}

void RelayPool::onClientDisconnected()
{
    if (m_shuttingDown)
        return;
    updateOnlineState();
}

void RelayPool::onClientEvent(const NostrEvent& ev)
{
    if (m_shuttingDown)
        return;

    // Dedup: skip events we've already forwarded.
    if (ev.id.isEmpty() || m_seenIds.contains(ev.id))
        return;

    m_seenIds.insert(ev.id);
    emit eventReceived(ev);
}

void RelayPool::onClientEose()
{
    if (m_shuttingDown)
        return;
    emit eose();
}

void RelayPool::onClientAck(const QString& eventId, bool accepted, const QString& msg)
{
    if (m_shuttingDown)
        return;
    emit publishAck(eventId, accepted, msg);
}

// ── Private helpers ────────────────────────────────────────────────────────

void RelayPool::updateOnlineState()
{
    if (m_shuttingDown)
        return;

    bool anyConnected = false;
    for (const auto& c : m_clients) {
        if (c->isShuttingDown())
            continue;
        if (c->isConnected()) {
            anyConnected = true;
            break;
        }
    }

    if (anyConnected != m_online) {
        m_online = anyConnected;
        emit onlineChanged(m_online);
    }
}

} // namespace net
