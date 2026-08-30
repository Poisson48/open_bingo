#pragma once

#include "../core/bingotypes.h"
#include "../net/relaypool.h"
#include "../store/database.h"

#include <QHash>
#include <QObject>
#include <QTimer>
#include <QHash>
#include <QSet>
#include <QString>
#include <QVariantList>
#include <map>
#include <optional>
#include <vector>

namespace app {

// Synchronise un projet bingo chiffré via relais Nostr (même transport que Colo Tâches).
// Chaque projet partagé possède une clé de canal ; les snapshots JSON sont publiés
// en kind 4545, chiffrés XChaCha20 comme dans Colo.
class ProjectSync : public QObject
{
    Q_OBJECT

public:
    explicit ProjectSync(QObject* parent = nullptr);

    void init(store::Database* db, net::RelayPool* pool, const QString& deviceId);

    void setAppInForeground(bool foreground) { m_appInForeground = foreground; }
    void setDeferBackgroundNotificationsToPush(bool defer) {
        m_deferBackgroundNotifs = defer;
    }

    Q_PROPERTY(bool online READ online NOTIFY onlineChanged)
    Q_PROPERTY(int pendingChanges READ pendingChanges NOTIFY pendingChangesChanged)

    bool online() const;
    int  pendingChanges() const;

    Q_INVOKABLE void enableSharing(const QString& projectId);
    // URI openbingo://join/1/… (génère la clé si besoin, sans la régénérer).
    Q_INVOKABLE QString joinUri(const QString& projectId, const QString& title);
    // Rejoint un projet ; renvoie l'id local ou chaîne vide si lien invalide.
    Q_INVOKABLE QString joinFromUri(const QString& uri);
    Q_INVOKABLE void onLocalProjectChange(const QString& projectId);
    Q_INVOKABLE void subscribeAll(int64_t since = 0);
    // Flush outbox + resubscribe (retour au premier plan / changement de relais).
    Q_INVOKABLE void catchUpOnForeground();
    Q_INVOKABLE void reconcileOutbox();
    // Arrête la sync locale (clé + abonnement) sans toucher les autres appareils.
    Q_INVOKABLE void leaveSharing(const QString& projectId);

    // Overlays à coller dans le prochain snapshot (cochage Play) — one-shot.
    void setOutboundPlayOverlays(const QString& projectId, const QVariantList& overlays);
    // Overlays reçus d'un pair (vidé après lecture). Vide si echo local / absent.
    QVariantList takeInboundPlayOverlays(const QString& projectId);
    // true une fois après un echo de notre propre publish (pas de rejeu notif).
    bool takeSkipOverlayRecompute(const QString& projectId);

public slots:
    void handleRelayEvent(const net::NostrEvent& ev);

signals:
    void onlineChanged();
    void pendingChangesChanged();
    void remoteProjectUpdated(const QString& projectId);
    void toast(const QString& message);

private slots:
    void onRelayOnline(bool online);
    void onPublishAck(const QString& eventId, bool accepted, const QString& msg);
    void onDebounce();
    void onAckWatchdog();

private:
    void publishSnapshot(const std::string& projectId);
    std::optional<std::string> channelTagFor(const std::string& projectId);
    std::optional<std::vector<uint8_t>> keyFor(const std::string& projectId);
    void flushOutbox();
    void reconcileStuckOutbox();
    void startOutboxReconcileTimer();
    void armAckWatchdog();
    void schedulePushWake(const std::string& projectId);
    void maybeSendPushWake(const std::string& projectId);

    store::Database* m_db   = nullptr;
    net::RelayPool*  m_pool = nullptr;
    QString          m_deviceId;

    QTimer           m_debounce;
    QTimer           m_ackWatchdog;
    QTimer           m_outboxReconcileTimer;
    QHash<QString, QTimer*> m_pushWakeTimers;
    QHash<QString, qint64>  m_outboxAddedMs;
    QSet<QString>    m_pendingProjects;
    QSet<QString>    m_subscribed;
    std::map<QString, std::string> m_pendingAcks;
    // Rejets OK par event id — toast seulement si tous les relais connectés ont refusé.
    QHash<QString, int> m_publishRejectCounts;
    // projectId → JSON array overlays (sortie / entrée sync).
    std::map<std::string, std::string> m_outboundPlayOverlays;
    std::map<std::string, std::string> m_inboundPlayOverlays;
    QSet<QString> m_skipOverlayRecompute;
    bool m_deferBackgroundNotifs = false;
    bool m_appInForeground       = true;
};

} // namespace app
