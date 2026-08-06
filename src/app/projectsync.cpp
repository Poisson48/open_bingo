#include "projectsync.h"

#include "../core/jsoncodec.h"
#include "../core/pairing.h"
#include "../net/crypto.h"
#include "../net/nostr.h"

#include <QDateTime>
#include <QDebug>
#include <QJsonArray>

namespace {

// Stub local créé à la jointure : updatedAt=0 pour que tout snapshot distant gagne le LWW.
// (Un stub horodaté « maintenant » rejetait le contenu hôte — seul le titre URI restait.)
core::Project makeJoinStub(const std::string& id, const std::string& title)
{
    core::Project p;
    p.id = id;
    p.title = title.empty() ? "Projet partagé" : title;
    p.createdAt = 0;
    p.updatedAt = 0;
    p.players.clear();
    return p;
}

} // namespace

namespace app {

ProjectSync::ProjectSync(QObject* parent)
    : QObject(parent)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(300);
    connect(&m_debounce, &QTimer::timeout, this, &ProjectSync::onDebounce);
}

void ProjectSync::init(store::Database* db, net::RelayPool* pool, const QString& deviceId)
{
    m_db = db;
    m_pool = pool;
    m_deviceId = deviceId;

    if (!m_pool)
        return;

    connect(m_pool, &net::RelayPool::eventReceived, this, &ProjectSync::handleRelayEvent);
    connect(m_pool, &net::RelayPool::onlineChanged, this, &ProjectSync::onRelayOnline);
    connect(m_pool, &net::RelayPool::publishAck, this, &ProjectSync::onPublishAck);

    m_pool->setRelays(net::RelayPool::defaultRelays());
    m_pool->connectAll();
}

bool ProjectSync::online() const
{
    return m_pool && m_pool->isOnline();
}

int ProjectSync::pendingChanges() const
{
    return m_db ? m_db->outboxCount() : 0;
}

void ProjectSync::enableSharing(const QString& projectId)
{
    if (!m_db || projectId.isEmpty())
        return;
    // Ne jamais régénérer la clé : sinon les participants déjà joints sont exclus.
    if (!m_db->getSyncKey(projectId.toStdString())) {
        auto key = net::generateListKey();
        if (key.size() != 32)
            return;
        m_db->setSyncKey(projectId.toStdString(), key);
    }
    // Horodater avant publish pour que les invités (même avec un vieux stub) acceptent.
    if (auto p = m_db->getProject(projectId.toStdString())) {
        p->updatedAt = QDateTime::currentMSecsSinceEpoch();
        m_db->upsertProject(*p);
    }
    subscribeAll(0);
    publishSnapshot(projectId.toStdString());
}

QString ProjectSync::joinUri(const QString& projectId, const QString& title)
{
    if (projectId.isEmpty())
        return {};
    if (!keyFor(projectId.toStdString()))
        enableSharing(projectId);
    else {
        // Republier à chaque ouverture du partage : rattrape les invités qui ont manqué l'event.
        if (auto p = m_db->getProject(projectId.toStdString())) {
            p->updatedAt = QDateTime::currentMSecsSinceEpoch();
            m_db->upsertProject(*p);
        }
        publishSnapshot(projectId.toStdString());
    }
    const auto key = keyFor(projectId.toStdString());
    if (!key)
        return {};
    return QString::fromStdString(
        core::buildJoinUri(projectId.toStdString(), *key, title.toStdString()));
}

QString ProjectSync::joinFromUri(const QString& uri)
{
    const auto info = core::parseJoinUri(uri.toStdString());
    if (!info || !m_db)
        return {};

    m_db->setSyncKey(info->listId, info->key);

    if (auto existing = m_db->getProject(info->listId)) {
        // Projet déjà connu : si c'est un stub (ou quasi vide), forcer updatedAt=0
        // pour accepter le prochain snapshot même après un ancien bug LWW.
        if (existing->cases.empty() && existing->grids.empty()
            && existing->updatedAt > 0) {
            existing->updatedAt = 0;
            existing->title = info->title.empty() ? existing->title : info->title;
            m_db->upsertProject(*existing);
        }
    } else {
        m_db->upsertProject(makeJoinStub(info->listId, info->title));
    }

    subscribeAll(0);
    const QString id = QString::fromStdString(info->listId);
    emit remoteProjectUpdated(id);
    emit toast(QStringLiteral("Projet rejoint : %1").arg(QString::fromStdString(info->title)));
    return id;
}

void ProjectSync::leaveSharing(const QString& projectId)
{
    if (!m_db || projectId.isEmpty())
        return;
    const auto tag = channelTagFor(projectId.toStdString());
    m_db->clearSyncKey(projectId.toStdString());
    if (tag) {
        const QString qtag = QString::fromStdString(*tag);
        m_subscribed.remove(qtag);
        if (m_pool)
            m_pool->unsubscribe(qtag);
    }
}

void ProjectSync::onLocalProjectChange(const QString& projectId)
{
    m_pendingProjects.insert(projectId);
    m_debounce.start();
}

void ProjectSync::subscribeAll(int64_t since)
{
    if (!m_db || !m_pool)
        return;
    for (const auto& id : m_db->sharedProjectIds()) {
        const auto tag = channelTagFor(id);
        if (!tag)
            continue;
        const QString qtag = QString::fromStdString(*tag);
        if (m_subscribed.contains(qtag))
            continue;
        m_pool->subscribeAll(qtag, since);
        m_subscribed.insert(qtag);
    }
}

void ProjectSync::handleRelayEvent(const net::NostrEvent& ev)
{
    if (!m_db || ev.kind != 4545)
        return;

    QString channelTag;
    for (const QJsonValue& tagVal : ev.tags) {
        const QJsonArray tag = tagVal.toArray();
        if (tag.size() >= 2 && tag.at(0).toString() == QLatin1String("t")) {
            channelTag = tag.at(1).toString();
            break;
        }
    }
    if (channelTag.isEmpty())
        return;

    if (m_db->isEventSeen(ev.id.toStdString()))
        return;
    m_db->markEventSeen(ev.id.toStdString());

    for (const auto& projectId : m_db->sharedProjectIds()) {
        const auto tag = channelTagFor(projectId);
        const auto key = keyFor(projectId);
        if (!tag || !key || *tag != channelTag.toStdString())
            continue;

        const auto plain = net::decryptPayload(*key, *tag, ev.content.toStdString());
        if (!plain)
            continue;

        bool ok = false;
        auto remote = core::JsonCodec::projectFromJson(*plain, &ok);
        if (!ok)
            continue;

        // Toujours rattacher l'id local (le JSON distant doit coïncider, mais on force).
        remote.id = projectId;

        auto local = m_db->getProject(projectId);
        const bool accept = !local
            || local->updatedAt == 0
            || remote.updatedAt >= local->updatedAt;
        if (accept) {
            m_db->upsertProject(remote);
            emit remoteProjectUpdated(QString::fromStdString(projectId));
        }
        return;
    }
}

void ProjectSync::onRelayOnline(bool online)
{
    Q_UNUSED(online);
    emit onlineChanged();
    if (online) {
        flushOutbox();
        subscribeAll(0);
    }
}

void ProjectSync::onPublishAck(const QString& eventId, bool accepted, const QString& msg)
{
    Q_UNUSED(msg);
    if (!accepted || !m_db)
        return;
    const auto it = m_pendingAcks.find(eventId);
    if (it == m_pendingAcks.end())
        return;
    m_db->outboxRemoveForEvent(eventId.toStdString());
    m_pendingAcks.erase(it);
    emit pendingChangesChanged();
}

void ProjectSync::onDebounce()
{
    for (const QString& id : std::as_const(m_pendingProjects))
        publishSnapshot(id.toStdString());
    m_pendingProjects.clear();
}

void ProjectSync::publishSnapshot(const std::string& projectId)
{
    if (!m_db || !m_pool)
        return;
    const auto project = m_db->getProject(projectId);
    const auto key = keyFor(projectId);
    const auto tag = channelTagFor(projectId);
    if (!project || !key || !tag)
        return;

    const std::string json = core::JsonCodec::projectToJson(*project, false);

    net::NostrEvent ev;
    ev.kind = 4545;
    ev.created_at = QDateTime::currentSecsSinceEpoch();
    ev.tags = QJsonArray{ QJsonArray{ QStringLiteral("t"), QString::fromStdString(*tag) } };
    ev.content = QString::fromStdString(net::encryptPayload(*key, *tag, json));

    const auto seed = net::deriveNostrSeed(*key);
    if (!net::signEvent(ev, seed))
        return;

    if (m_pool->isOnline()) {
        m_pool->publishToAll(ev);
        m_pendingAcks[ev.id] = projectId;
    } else {
        m_db->outboxPush(projectId, ev.id.toStdString(), ev.content.toStdString());
        emit pendingChangesChanged();
    }
}

std::optional<std::string> ProjectSync::channelTagFor(const std::string& projectId)
{
    const auto key = keyFor(projectId);
    if (!key)
        return std::nullopt;
    return net::deriveChannelTag(*key);
}

std::optional<std::vector<uint8_t>> ProjectSync::keyFor(const std::string& projectId)
{
    if (!m_db)
        return std::nullopt;
    return m_db->getSyncKey(projectId);
}

void ProjectSync::flushOutbox()
{
    if (!m_db || !m_pool)
        return;
    for (const auto& row : m_db->outboxPeekAll()) {
        const auto key = keyFor(row.projectId);
        const auto tag = channelTagFor(row.projectId);
        if (!key || !tag)
            continue;

        net::NostrEvent ev;
        ev.kind = 4545;
        ev.created_at = QDateTime::currentSecsSinceEpoch();
        ev.tags = QJsonArray{ QJsonArray{ QStringLiteral("t"), QString::fromStdString(*tag) } };
        ev.content = QString::fromStdString(row.content);
        const auto seed = net::deriveNostrSeed(*key);
        if (!net::signEvent(ev, seed))
            continue;
        m_pool->publishToAll(ev);
        m_pendingAcks[ev.id] = row.projectId;
    }
}

} // namespace app
