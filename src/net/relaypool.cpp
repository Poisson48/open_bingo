#include "relaypool.h"

#include <QDateTime>
#include <QDebug>

namespace net {

RelayPool::RelayPool(QObject* parent)
    : QObject(parent)
{}

// static
QList<QUrl> RelayPool::defaultRelays()
{
    return {
        QUrl(QStringLiteral("wss://colo-apps.les-crevettes-cevenoles.fr")),
    };
}

void RelayPool::setRelays(const QList<QUrl>& urls)
{
    // Disconnect old clients.
    for (auto& c : m_clients)
        c->disconnectFromRelay();
    m_clients.clear();

    for (const QUrl& url : urls) {
        auto client = std::make_unique<RelayClient>(url, nullptr); // unique_ptr owns

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

    // Re-apply subscription if one is already active.
    if (!m_channelTags.isEmpty()) {
        for (auto& c : m_clients)
            c->subscribe(m_channelTags, m_since);
    }
}

void RelayPool::connectAll()
{
    for (auto& c : m_clients)
        c->connectToRelay();
}

void RelayPool::disconnectAll()
{
    for (auto& c : m_clients)
        c->disconnectFromRelay();
}

int RelayPool::publishToAll(const NostrEvent& ev)
{
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
    if (channelTag.isEmpty())
        return;
    if (!m_channelTags.contains(channelTag))
        m_channelTags.append(channelTag);
    m_since = since;
    for (auto& c : m_clients)
        c->subscribe(m_channelTags, since);
}

void RelayPool::unsubscribe(const QString& channelTag)
{
    if (!m_channelTags.contains(channelTag))
        return;
    m_channelTags.removeAll(channelTag);
    for (auto& c : m_clients) {
        if (m_channelTags.isEmpty())
            c->closeSubscription();
        else
            c->subscribe(m_channelTags, m_since);
    }
}

// ── Private slots ──────────────────────────────────────────────────────────

void RelayPool::onClientConnected()
{
    updateOnlineState();
}

void RelayPool::onClientDisconnected()
{
    updateOnlineState();
}

void RelayPool::onClientEvent(const NostrEvent& ev)
{
    // Dedup: skip events we've already forwarded.
    if (ev.id.isEmpty() || m_seenIds.contains(ev.id))
        return;

    m_seenIds.insert(ev.id);
    emit eventReceived(ev);
}

void RelayPool::onClientEose()
{
    emit eose();
}

void RelayPool::onClientAck(const QString& eventId, bool accepted, const QString& msg)
{
    emit publishAck(eventId, accepted, msg);
}

// ── Private helpers ────────────────────────────────────────────────────────

void RelayPool::updateOnlineState()
{
    bool anyConnected = false;
    for (const auto& c : m_clients) {
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
