#include "projectsync.h"

#include "../core/jsoncodec.h"
#include "../core/pairing.h"
#include "../core/sharecodec.h"
#include "../net/crypto.h"
#include "../net/nostr.h"

#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>

namespace {

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

bool isContentEmpty(const core::Project& p)
{
    return p.cases.empty() && p.grids.empty() && p.players.empty();
}

// Snapshot Nostr : JSON compressé (z:/b64:) puis chiffré — les grilles bingo
// dépassent sinon la limite des relais (~100–128 Ko). Anciens events = JSON brut.
std::optional<std::string> plainToProjectJson(const std::string& plain)
{
    if (plain.rfind("z:", 0) == 0 || plain.rfind("b64:", 0) == 0) {
        bool ok = false;
        std::string json = core::ShareCodec::decompress(plain, &ok);
        if (!ok || json.empty())
            return std::nullopt;
        return json;
    }
    return plain;
}

} // namespace

namespace app {

ProjectSync::ProjectSync(QObject* parent)
    : QObject(parent)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(300);
    connect(&m_debounce, &QTimer::timeout, this, &ProjectSync::onDebounce);

    // Comme Colo : si aucun OK n'arrive, on force une reconnexion (Android WS zombies).
    m_ackWatchdog.setSingleShot(true);
    m_ackWatchdog.setInterval(8000);
    connect(&m_ackWatchdog, &QTimer::timeout, this, &ProjectSync::onAckWatchdog);
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
    if (!m_db->getSyncKey(projectId.toStdString())) {
        auto key = net::generateListKey();
        if (key.size() != 32)
            return;
        m_db->setSyncKey(projectId.toStdString(), key);
    }
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
    // Colo : le QR ne porte que la clé. On republie quand même un snapshot pour
    // que l'invité trouve l'historique (bingo = un gros event, pas un flux de deltas).
    if (m_pool)
        m_pool->connectAll();

    if (!keyFor(projectId.toStdString()))
        enableSharing(projectId);
    else {
        if (auto p = m_db->getProject(projectId.toStdString())) {
            p->updatedAt = QDateTime::currentMSecsSinceEpoch();
            m_db->upsertProject(*p);
        }
        publishSnapshot(projectId.toStdString());
    }
    const auto key = keyFor(projectId.toStdString());
    if (!key)
        return {};

    if (!online()) {
        emit toast(QStringLiteral(
            "Hors ligne — le lien est prêt, mais le contenu partira "
            "quand un relais sera joignable. Gardez l'app ouverte."));
    } else if (pendingChanges() > 0) {
        emit toast(QStringLiteral("Envoi du contenu aux relais…"));
    } else {
        // publishSnapshot a pu no-op (projet vide) : le signaler.
        const auto p = m_db ? m_db->getProject(projectId.toStdString()) : std::nullopt;
        if (!p || isContentEmpty(*p)) {
            emit toast(QStringLiteral(
                "Projet sans phrases/joueurs/grilles — rien à synchroniser."));
        }
    }

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
        if (existing->cases.empty() && existing->grids.empty()
            && existing->updatedAt > 0) {
            existing->updatedAt = 0;
            existing->title = info->title.empty() ? existing->title : info->title;
            m_db->upsertProject(*existing);
        }
    } else {
        m_db->upsertProject(makeJoinStub(info->listId, info->title));
    }

    const auto tag = channelTagFor(info->listId);
    if (tag)
        m_subscribed.remove(QString::fromStdString(*tag));
    subscribeAll(0);

    const QString id = QString::fromStdString(info->listId);
    emit remoteProjectUpdated(id);
    emit toast(QStringLiteral("Projet rejoint : %1 — sync du contenu…")
                   .arg(QString::fromStdString(info->title)));

    QTimer::singleShot(12000, this, [this, id]() {
        if (!m_db)
            return;
        const auto p = m_db->getProject(id.toStdString());
        if (!p || !isContentEmpty(*p))
            return;
        emit toast(QStringLiteral(
            "Toujours vide — l'hôte doit rouvrir « Partager » (en ligne) "
            "pour renvoyer le contenu via les relais."));
        const auto tag = channelTagFor(id.toStdString());
        if (tag) {
            m_subscribed.remove(QString::fromStdString(*tag));
            subscribeAll(0);
        }
    });
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

    for (const auto& projectId : m_db->sharedProjectIds()) {
        const auto tag = channelTagFor(projectId);
        const auto key = keyFor(projectId);
        if (!tag || !key || *tag != channelTag.toStdString())
            continue;

        const auto plain = net::decryptPayload(*key, *tag, ev.content.toStdString());
        if (!plain)
            continue;

        const auto json = plainToProjectJson(*plain);
        if (!json)
            continue;

        bool ok = false;
        auto remote = core::JsonCodec::projectFromJson(*json, &ok);
        if (!ok)
            continue;

        remote.id = projectId;

        auto local = m_db->getProject(projectId);
        const bool accept = !local
            || local->updatedAt == 0
            || (isContentEmpty(*local) && !isContentEmpty(remote))
            || remote.updatedAt >= local->updatedAt;
        if (accept) {
            m_db->markEventSeen(ev.id.toStdString());
            m_db->upsertProject(remote);
            // Echo de notre propre publish = confirmation relais (sans attendre OK).
            const auto ackIt = m_pendingAcks.find(ev.id);
            if (ackIt != m_pendingAcks.end()) {
                m_db->outboxRemoveForEvent(ev.id.toStdString());
                m_pendingAcks.erase(ackIt);
                if (m_pendingAcks.empty() && m_db->outboxCount() == 0) {
                    m_ackWatchdog.stop();
                    emit toast(QStringLiteral(
                        "Contenu publié — les invités peuvent synchroniser."));
                }
                emit pendingChangesChanged();
            }
            emit remoteProjectUpdated(QString::fromStdString(projectId));
            if (!isContentEmpty(remote) && local && isContentEmpty(*local)) {
                emit toast(QStringLiteral("Contenu synchronisé : %1")
                               .arg(QString::fromStdString(remote.title)));
            }
        }
        return;
    }
}

void ProjectSync::onRelayOnline(bool online)
{
    emit onlineChanged();
    if (!online)
        return;
    // Ne pas seulement rejouer l'outbox : un event pré-2.0.33 « too large »
    // resterait bloqué. Republier un snapshot compressé frais.
    if (m_db) {
        for (const auto& id : m_db->sharedProjectIds()) {
            if (auto p = m_db->getProject(id)) {
                if (!isContentEmpty(*p))
                    publishSnapshot(id);
            }
        }
    }
    flushOutbox();
    subscribeAll(0);
}

void ProjectSync::onPublishAck(const QString& eventId, bool accepted, const QString& msg)
{
    if (!m_db)
        return;
    const auto it = m_pendingAcks.find(eventId);
    if (it == m_pendingAcks.end())
        return;
    if (!accepted) {
        qWarning() << "[ProjectSync] publish rejected" << eventId.left(12) << msg;
        const std::string projectId = it->second;
        m_pendingAcks.erase(it);
        if (msg.contains(QStringLiteral("too large"), Qt::CaseInsensitive)) {
            // Abandonner l'event trop gros et republier compressé (z:).
            m_db->outboxRemoveForEvent(eventId.toStdString());
            emit pendingChangesChanged();
            publishSnapshot(projectId);
            return;
        }
        emit toast(QStringLiteral("Relais a refusé l'envoi : %1")
                       .arg(msg.isEmpty() ? QStringLiteral("erreur") : msg));
        emit pendingChangesChanged();
        return;
    }
    // Comme Colo : retirer l'entrée outbox dont l'event id correspond.
    m_db->outboxRemoveForEvent(eventId.toStdString());
    m_db->markEventSeen(eventId.toStdString());
    m_pendingAcks.erase(it);
    if (m_pendingAcks.empty() && m_db->outboxCount() == 0) {
        m_ackWatchdog.stop();
        emit toast(QStringLiteral("Contenu publié — les invités peuvent synchroniser."));
    }
    emit pendingChangesChanged();
}

void ProjectSync::onAckWatchdog()
{
    if (!m_db || !m_pool || m_db->outboxCount() == 0)
        return;
    // Pas d'OK : souvent une socket WS « Connected » morte sur mobile.
    qWarning() << "[ProjectSync] ack timeout — forceReconnect + republish"
               << "pending=" << m_db->outboxCount()
               << "connected=" << m_pool->connectedCount();
    emit toast(QStringLiteral(
        "Les relais ne répondent pas — nouvelle tentative d'envoi…"));
    m_pendingAcks.clear();
    m_pool->forceReconnect();
    // Nouveau snapshot (nouvel id) plutôt que rejouer un event peut-être corrompu.
    for (const auto& id : m_db->sharedProjectIds()) {
        if (auto p = m_db->getProject(id)) {
            if (!isContentEmpty(*p))
                publishSnapshot(id);
        }
    }
}

void ProjectSync::armAckWatchdog()
{
    m_ackWatchdog.start();
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
    if (isContentEmpty(*project))
        return;

    const std::string json = core::JsonCodec::projectToJson(*project, false);
    // Compresse avant chiffrement : grilles × joueurs font exploser le JSON.
    const std::string packed = core::ShareCodec::compress(json);

    net::NostrEvent ev;
    ev.kind = 4545;
    ev.created_at = QDateTime::currentSecsSinceEpoch();
    // NIP-01 : tags = [["t","channel"], ...]. Pas de QJsonArray{ QJsonArray{…} } :
    // avec un seul élément du même type, Clang/NDK peut prendre le copy-ctor et
    // aplatir en ["t","channel"] → rejet relais « tag in tags field was not an array ».
    {
        QJsonArray tagT;
        tagT.append(QStringLiteral("t"));
        tagT.append(QString::fromStdString(*tag));
        ev.tags.append(tagT);
    }
    const std::string cipher = net::encryptPayload(*key, *tag, packed);
    if (cipher.empty()) {
        qWarning() << "[ProjectSync] encryptPayload failed — not publishing";
        emit toast(QStringLiteral("Échec du chiffrement — partage impossible."));
        return;
    }
    ev.content = QString::fromStdString(cipher);

    const auto seed = net::deriveNostrSeed(*key);
    if (!net::signEvent(ev, seed)) {
        qWarning() << "[ProjectSync] signEvent failed — not publishing";
        emit toast(QStringLiteral("Échec de signature — partage impossible."));
        return;
    }

    // Comme Colo SyncEngine : outbox = event JSON complet, retiré seulement à l'OK.
    // Un seul snapshot en attente par projet (LWW document entier).
    // Ne PAS markEventSeen avant ACK : sinon on ne peut pas confirmer l'echo relais,
    // et un OK fantôme laisse croire que c'est parti alors que le canal est vide.
    const std::string eventJson =
        QJsonDocument(ev.toJson()).toJson(QJsonDocument::Compact).toStdString();
    m_db->outboxPush(projectId, ev.id.toStdString(), eventJson);
    emit pendingChangesChanged();
    m_pendingAcks[ev.id] = projectId;

    const int sent = m_pool->isOnline() ? m_pool->publishToAll(ev) : 0;
    if (sent == 0) {
        qWarning() << "[ProjectSync] publish with 0 connected relays — force reconnect";
        emit toast(QStringLiteral("Aucun relais joignable — reconnexion…"));
        m_pool->forceReconnect();
    } else {
        qDebug() << "[ProjectSync] published to" << sent << "relay(s) id=" << ev.id.left(12);
    }
    armAckWatchdog();
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
    if (!m_db || !m_pool || !m_pool->isOnline())
        return;
    // Republier le MÊME event (même id) — pas de re-sign (sinon l'ACK ne matche plus).
    for (const auto& row : m_db->outboxPeekAll()) {
        const QJsonDocument doc = QJsonDocument::fromJson(
            QByteArray::fromStdString(row.content));
        if (!doc.isObject()) {
            m_db->outboxRemoveForEvent(row.eventId);
            continue;
        }
        auto evOpt = net::NostrEvent::fromJson(doc.object());
        if (!evOpt) {
            m_db->outboxRemoveForEvent(row.eventId);
            continue;
        }
        m_pool->publishToAll(*evOpt);
        m_pendingAcks[evOpt->id] = row.projectId;
    }
    emit pendingChangesChanged();
    if (!m_pendingAcks.empty())
        armAckWatchdog();
}

} // namespace app
