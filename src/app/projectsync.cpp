#include "projectsync.h"

#include "../core/jsoncodec.h"
#include "../core/pairing.h"
#include "../core/sharecodec.h"
#include "../net/crypto.h"
#include "../net/nostr.h"
#include "../net/pushclient.h"

#include <QHash>
#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

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

bool tagsAreNestedArrays(const QJsonArray& tags)
{
    if (tags.isEmpty())
        return false;
    for (const QJsonValue& v : tags) {
        if (!v.isArray())
            return false;
    }
    return true;
}

bool isCorruptTagRejection(const QString& msg)
{
    const QString m = msg.toLower();
    return m.contains(QStringLiteral("tag"))
        && (m.contains(QStringLiteral("not an array"))
            || m.contains(QStringLiteral("was not an array"))
            || m.contains(QStringLiteral("tags field")));
}

void appendChannelTag(net::NostrEvent& ev, const QString& channel)
{
    // NIP-01 : tags = [["t","channel"], ...]. Pas de QJsonArray{ QJsonArray{…} } :
    // Clang/NDK peut aplatir en ["t","channel"] → rejet relais.
    QJsonArray tagT;
    tagT.append(QStringLiteral("t"));
    tagT.append(channel);
    ev.tags.append(tagT);
}

} // namespace

namespace {

bool relayAckMeansStored(bool accepted, const QString& msg)
{
    if (accepted)
        return true;
    const QString lower = msg.toLower();
    return lower.contains(QStringLiteral("duplicate"))
        || lower.contains(QStringLiteral("already"))
        || lower.contains(QStringLiteral("stored"));
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

    m_outboxReconcileTimer.setInterval(15'000);
    connect(&m_outboxReconcileTimer, &QTimer::timeout, this, [this] {
        reconcileStuckOutbox();
        if (m_db && m_db->outboxCount() == 0)
            m_outboxReconcileTimer.stop();
    });
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
    m_outboundPlayOverlays.erase(projectId.toStdString());
    m_inboundPlayOverlays.erase(projectId.toStdString());
}

void ProjectSync::setOutboundPlayOverlays(const QString& projectId, const QVariantList& overlays)
{
    if (projectId.isEmpty())
        return;
    if (overlays.isEmpty()) {
        m_outboundPlayOverlays.erase(projectId.toStdString());
        return;
    }
    const QJsonDocument doc(QJsonArray::fromVariantList(overlays));
    m_outboundPlayOverlays[projectId.toStdString()] =
        doc.toJson(QJsonDocument::Compact).toStdString();
}

QVariantList ProjectSync::takeInboundPlayOverlays(const QString& projectId)
{
    const auto it = m_inboundPlayOverlays.find(projectId.toStdString());
    if (it == m_inboundPlayOverlays.end())
        return {};
    const std::string json = it->second;
    m_inboundPlayOverlays.erase(it);
    if (json.empty())
        return {};
    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(json));
    if (!doc.isArray())
        return {};
    return doc.array().toVariantList();
}

bool ProjectSync::takeSkipOverlayRecompute(const QString& projectId)
{
    if (!m_skipOverlayRecompute.contains(projectId))
        return false;
    m_skipOverlayRecompute.remove(projectId);
    return true;
}

void ProjectSync::onLocalProjectChange(const QString& projectId)
{
    m_pendingProjects.insert(projectId);
    m_debounce.start();
    schedulePushWake(projectId.toStdString());
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

void ProjectSync::catchUpOnForeground()
{
    if (!m_pool)
        return;
    if (!m_pool->isOnline())
        m_pool->connectAll();
    if (!m_pool->isOnline())
        return;
    flushOutbox();
    reconcileOutbox();
    subscribeAll(0);
}

void ProjectSync::reconcileOutbox()
{
    reconcileStuckOutbox();
}

void ProjectSync::startOutboxReconcileTimer()
{
    if (!m_outboxReconcileTimer.isActive())
        m_outboxReconcileTimer.start();
}

void ProjectSync::reconcileStuckOutbox()
{
    if (!m_db)
        return;

    static constexpr int64_t kOutboxStaleMs = 45'000;
    const int64_t now = QDateTime::currentMSecsSinceEpoch();
    const bool online = m_pool && m_pool->isOnline();
    bool changed = false;

    for (const auto& row : m_db->outboxPeekAll()) {
        const QString eventId = QString::fromStdString(row.eventId);
        const qint64 created = m_outboxAddedMs.value(eventId, now);
        if (now - created < kOutboxStaleMs)
            continue;

        const QJsonDocument doc = QJsonDocument::fromJson(
            QByteArray::fromStdString(row.content));
        if (!doc.isObject()) {
            m_db->outboxRemoveForEvent(row.eventId);
            m_outboxAddedMs.remove(eventId);
            changed = true;
            continue;
        }
        const auto evOpt = net::NostrEvent::fromJson(doc.object());
        if (!evOpt) {
            m_db->outboxRemoveForEvent(row.eventId);
            m_outboxAddedMs.remove(eventId);
            changed = true;
            continue;
        }

        if (!online)
            continue;

        if (m_pendingAcks.find(evOpt->id) == m_pendingAcks.end()) {
            qInfo() << "[ProjectSync] purging stale delivered outbox entry"
                    << evOpt->id.left(12);
            m_db->outboxRemoveForEvent(row.eventId);
            m_outboxAddedMs.remove(eventId);
            changed = true;
            continue;
        }

        qInfo() << "[ProjectSync] purging stale unacked outbox entry"
                << evOpt->id.left(12);
        m_db->outboxRemoveForEvent(row.eventId);
        m_pendingAcks.erase(evOpt->id);
        m_outboxAddedMs.remove(eventId);
        changed = true;
    }

    if (changed)
        emit pendingChangesChanged();
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

    if (m_db->isEventSeen(ev.id.toStdString())) {
        m_db->outboxRemoveForEvent(ev.id.toStdString());
        emit pendingChangesChanged();
        return;
    }

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
        auto bundle = core::JsonCodec::projectBundleFromJson(*json, &ok);
        if (!ok)
            continue;

        auto& remote = bundle.project;
        remote.id = projectId;

        auto local = m_db->getProject(projectId);
        const bool accept = !local
            || local->updatedAt == 0
            || (isContentEmpty(*local) && !isContentEmpty(remote))
            || remote.updatedAt >= local->updatedAt;
        if (accept) {
            m_db->markEventSeen(ev.id.toStdString());
            m_db->upsertProject(remote);
            // Ancien snapshot sans playChecks : ne pas écraser les coches locales.
            if (bundle.hasPlayChecks)
                m_db->replaceAllPlayChecks(projectId, bundle.playChecks);
            // Echo de notre propre publish = confirmation relais (sans attendre OK).
            const auto ackIt = m_pendingAcks.find(ev.id);
            const bool ownEcho = ackIt != m_pendingAcks.end();
            if (ownEcho) {
                m_db->outboxRemoveForEvent(ev.id.toStdString());
                m_pendingAcks.erase(ackIt);
                if (m_pendingAcks.empty() && m_db->outboxCount() == 0) {
                    m_ackWatchdog.stop();
                    emit toast(QStringLiteral(
                        "Contenu publié — les invités peuvent synchroniser."));
                }
                emit pendingChangesChanged();
                // Pas de rejeu UI des overlays : déjà montrés localement au cochage.
                m_inboundPlayOverlays.erase(projectId);
            } else if (bundle.hasPlayOverlays && !bundle.playOverlaysJson.empty()) {
                // Pair distant : mêmes noms/gages que l'appareil qui a coché.
                m_inboundPlayOverlays[projectId] = bundle.playOverlaysJson;
            } else {
                m_inboundPlayOverlays.erase(projectId);
            }
            // Marqueur : l'UI ne doit pas recalculer les overlays sur un echo local.
            if (ownEcho)
                m_skipOverlayRecompute.insert(QString::fromStdString(projectId));
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
    flushOutbox();
    reconcileStuckOutbox();
    subscribeAll(0);
}

void ProjectSync::onPublishAck(const QString& eventId, bool accepted, const QString& msg)
{
    if (!m_db || eventId.isEmpty())
        return;

    if (relayAckMeansStored(accepted, msg)) {
        m_publishRejectCounts.remove(eventId);
        const auto it = m_pendingAcks.find(eventId);
        std::string projectId;
        if (it != m_pendingAcks.end()) {
            projectId = it->second;
            m_pendingAcks.erase(it);
        }

        m_db->outboxRemoveForEvent(eventId.toStdString());
        m_db->markEventSeen(eventId.toStdString());
        if (m_pendingAcks.empty() && m_db->outboxCount() == 0) {
            m_ackWatchdog.stop();
            emit toast(QStringLiteral("Contenu publié — les invités peuvent synchroniser."));
        }
        emit pendingChangesChanged();
        if (!projectId.empty())
            maybeSendPushWake(projectId);
        return;
    }

    qWarning() << "[ProjectSync] publish rejected" << eventId.left(12) << msg;
    const auto it = m_pendingAcks.find(eventId);
    if (it == m_pendingAcks.end())
        return;

    const std::string projectId = it->second;
    const bool rebuild = msg.contains(QStringLiteral("too large"), Qt::CaseInsensitive)
        || isCorruptTagRejection(msg);
    if (rebuild) {
        m_publishRejectCounts.remove(eventId);
        m_pendingAcks.erase(it);
        m_db->outboxRemoveForEvent(eventId.toStdString());
        emit pendingChangesChanged();
        publishSnapshot(projectId);
        return;
    }

    // Comme Colo Course : un rejet isolé n'est pas fatal — un autre relais peut accepter.
    const int rejections = ++m_publishRejectCounts[eventId];
    const int connected = m_pool ? m_pool->connectedCount() : 0;
    const int targets = connected > 0 ? connected : 1;
    if (rejections < targets)
        return;

    m_publishRejectCounts.remove(eventId);
    m_pendingAcks.erase(it);
    emit toast(QStringLiteral("Relais a refusé l'envoi : %1")
                   .arg(msg.isEmpty() ? QStringLiteral("erreur") : msg));
    emit pendingChangesChanged();
}

void ProjectSync::onAckWatchdog()
{
    if (!m_db || !m_pool || m_db->outboxCount() == 0)
        return;
    qWarning() << "[ProjectSync] ack timeout — forceReconnect + republish"
               << "pending=" << m_db->outboxCount()
               << "connected=" << m_pool->connectedCount();
    if (m_pool->forceReconnectAllowed()) {
        emit toast(QStringLiteral(
            "Les relais ne répondent pas — nouvelle tentative d'envoi…"));
        m_pendingAcks.clear();
        m_publishRejectCounts.clear();
        m_pool->forceReconnect();
    }
    // Reconstruire un snapshot frais (tags corrects) plutôt que rejouer un event
    // peut-être corrompu encore en outbox.
    for (const auto& row : m_db->outboxPeekAll()) {
        m_db->outboxRemoveForEvent(row.eventId);
        if (auto p = m_db->getProject(row.projectId)) {
            if (!isContentEmpty(*p))
                publishSnapshot(row.projectId);
        }
    }
    emit pendingChangesChanged();
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

    core::ProjectBundle bundle;
    bundle.project = *project;
    bundle.playChecks = m_db->getAllPlayChecks(projectId);
    bundle.hasPlayChecks = true;
    const auto ovIt = m_outboundPlayOverlays.find(projectId);
    if (ovIt != m_outboundPlayOverlays.end()) {
        bundle.hasPlayOverlays = true;
        bundle.playOverlaysJson = ovIt->second;
        m_outboundPlayOverlays.erase(ovIt); // one-shot
    }
    const std::string json = core::JsonCodec::projectBundleToJson(bundle, false);
    // Compresse avant chiffrement : grilles × joueurs font exploser le JSON.
    const std::string packed = core::ShareCodec::compress(json);

    net::NostrEvent ev;
    ev.kind = 4545;
    ev.created_at = QDateTime::currentSecsSinceEpoch();
    appendChannelTag(ev, QString::fromStdString(*tag));
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
    const std::string eventJson =
        QJsonDocument(ev.toJson()).toJson(QJsonDocument::Compact).toStdString();
    m_db->outboxPush(projectId, ev.id.toStdString(), eventJson);
    m_outboxAddedMs[ev.id] = QDateTime::currentMSecsSinceEpoch();
    emit pendingChangesChanged();
    m_pendingAcks[ev.id] = projectId;
    startOutboxReconcileTimer();

    const int sent = m_pool->isOnline() ? m_pool->publishToAll(ev) : 0;
    if (sent == 0) {
        qWarning() << "[ProjectSync] publish with 0 connected relays — force reconnect";
        if (m_pool->forceReconnectAllowed()) {
            emit toast(QStringLiteral("Aucun relais joignable — reconnexion…"));
            m_pool->forceReconnect();
        }
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
    // Sauf tags plats (bug Clang pré-2.0.35) : reconstruire un snapshot frais.
    for (const auto& row : m_db->outboxPeekAll()) {
        const QJsonDocument doc = QJsonDocument::fromJson(
            QByteArray::fromStdString(row.content));
        if (!doc.isObject()) {
            m_db->outboxRemoveForEvent(row.eventId);
            publishSnapshot(row.projectId);
            continue;
        }
        auto evOpt = net::NostrEvent::fromJson(doc.object());
        if (!evOpt) {
            m_db->outboxRemoveForEvent(row.eventId);
            publishSnapshot(row.projectId);
            continue;
        }
        if (!tagsAreNestedArrays(evOpt->tags)) {
            qWarning() << "[ProjectSync] dropping flat-tag outbox event"
                       << QString::fromStdString(row.eventId).left(12);
            m_db->outboxRemoveForEvent(row.eventId);
            m_pendingAcks.erase(QString::fromStdString(row.eventId));
            publishSnapshot(row.projectId);
            continue;
        }
        m_pool->publishToAll(*evOpt);
        m_pendingAcks[evOpt->id] = row.projectId;
        m_outboxAddedMs[evOpt->id] = QDateTime::currentMSecsSinceEpoch();
    }
    emit pendingChangesChanged();
    if (!m_pendingAcks.empty()) {
        armAckWatchdog();
        startOutboxReconcileTimer();
    }
}

void ProjectSync::schedulePushWake(const std::string& projectId)
{
    const QString key = QString::fromStdString(projectId);
    QTimer*& timer = m_pushWakeTimers[key];
    if (!timer) {
        timer = new QTimer(this);
        timer->setSingleShot(true);
        timer->setInterval(60'000);
        connect(timer, &QTimer::timeout, this, [this, projectId]() {
            maybeSendPushWake(projectId);
        });
    }
    timer->start();
}

void ProjectSync::maybeSendPushWake(const std::string& projectId)
{
    if (!m_db)
        return;

    if (m_deferBackgroundNotifs && !m_appInForeground)
        return;

    const auto enabled = m_db->getSetting("pushEnabled");
    if (enabled && *enabled == "0")
        return;

    const auto urlOpt = m_db->getSetting("pushBaseUrl");
    const QString base =
        urlOpt && !urlOpt->empty()
            ? QString::fromStdString(*urlOpt)
            : QStringLiteral("https://colo-apps.les-crevettes-cevenoles.fr/ntfy");

    const auto tagOpt = channelTagFor(projectId);
    if (!tagOpt)
        return;

    const auto projectOpt = m_db->getProject(projectId);
    const QString title =
        projectOpt ? QString::fromStdString(projectOpt->title)
                   : QString::fromStdString(projectId);

    net::sendPushWake(base,
                      net::pushTopicForChannel(QString::fromStdString(*tagOpt)),
                      title,
                      m_deviceId);
}

} // namespace app
