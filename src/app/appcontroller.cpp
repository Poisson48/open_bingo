#include "appcontroller.h"
#include "playlogic.h"
#include "exportprint.h"

#include "../core/generator.h"
#include "../core/jsoncodec.h"
#include "../core/projectcsv.h"
#include "../core/sharecodec.h"
#include "../net/crypto.h"
#include "../net/pushclient.h"
#include "../net/relaypool.h"
#include "platform.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QRegularExpression>
#include <Qt>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QSet>
#include <QJsonDocument>
#include <QPageLayout>
#include <QPageSize>
#include <QPrinter>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QUrl>
#include <QtGlobal>
#ifndef Q_OS_ANDROID
#  include <QPrintPreviewDialog>
#endif

namespace app {
namespace {

QVariantMap comboToMap(const core::ComboGages& c)
{
    return {
        { QStringLiteral("line"), QString::fromStdString(c.line) },
        { QStringLiteral("column"), QString::fromStdString(c.column) },
        { QStringLiteral("diagonal"), QString::fromStdString(c.diagonal) },
    };
}

QVariantMap multToMap(const core::Multipliers& m)
{
    return {
        { QStringLiteral("line"), m.line },
        { QStringLiteral("column"), m.column },
        { QStringLiteral("diagonal"), m.diagonal },
        { QStringLiteral("full"), m.full },
    };
}

} // namespace

AppController::AppController(QObject* parent)
    : QObject(parent)
{
    m_autoSaveTimer.setSingleShot(true);
    m_autoSaveTimer.setInterval(400);
    connect(&m_autoSaveTimer, &QTimer::timeout, this, &AppController::saveCurrentProject);
}

AppController::~AppController()
{
    m_autoSaveTimer.stop();
    if (m_mcp)
        m_mcp->setEnabled(false);
    m_mcp.reset();
    m_film.reset();
    m_projectSync.reset();
    m_relayPool.reset();
    m_updater.reset();
    m_projectModel.reset();
    if (m_db)
        m_db->close();
    m_db.reset();
}

QString AppController::databasePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/openbingo.db");
}

bool AppController::init()
{
    const bool screenshotMode = !qEnvironmentVariableIsEmpty("BINGO_SCREENSHOT_DIR");

    m_db = std::make_unique<store::Database>();
    if (!m_db->open(databasePath()))
        return false;

    // unique_ptr ownership only — no QObject parent (avoid double-free).
    m_projectModel = std::make_unique<ProjectModel>(nullptr);
    m_relayPool = std::make_unique<net::RelayPool>(nullptr);
    m_projectSync = std::make_unique<ProjectSync>(nullptr);
    m_updater = std::make_unique<Updater>(nullptr);
    m_mcp = std::make_unique<McpServer>(this);   // controller*, parent=nullptr
    m_film = std::make_unique<FilmAssistant>(this); // controller*, parent=nullptr

    std::string deviceIdStr = m_db->getSetting("device_id").value_or("");
    if (deviceIdStr.empty()) {
        deviceIdStr = core::JsonCodec::makeId();
        m_db->setSetting("device_id", deviceIdStr);
    }
    const QString deviceId = QString::fromStdString(deviceIdStr);
    m_deviceId = deviceId;

    if (!screenshotMode) {
        m_projectSync->init(m_db.get(), m_relayPool.get(), deviceId);

        static const QString kLegacyRelays =
            QStringLiteral("wss://relay.damus.io,wss://nos.lol,wss://relay.nostr.band,"
                           "wss://offchain.pub,wss://relay.primal.net");
        static const QString kColoOnlyRelays =
            QStringLiteral("wss://colo-apps.les-crevettes-cevenoles.fr");
        auto relaysSetting = m_db->getSetting("relays");
        QList<QUrl> relayUrls;
        if (relaysSetting && !relaysSetting->empty()) {
            QString relaysStr = QString::fromStdString(*relaysSetting);
            if (relaysStr == kLegacyRelays || relaysStr == kColoOnlyRelays)
                relaysStr.clear();
            for (const QString &u : relaysStr.split(QLatin1Char(','), Qt::SkipEmptyParts))
                relayUrls.append(QUrl(u.trimmed()));
        }
        if (relayUrls.isEmpty()) {
            relayUrls = net::RelayPool::defaultRelays();
            QStringList parts;
            for (const QUrl &u : relayUrls)
                parts.append(u.toString());
            m_db->setSetting("relays", parts.join(QLatin1Char(',')).toStdString());
        }
        m_relayPool->setRelays(relayUrls);
        m_relayPool->connectAll();
        m_projectSync->subscribeAll(0);

        connect(m_projectSync.get(), &ProjectSync::toast, this, &AppController::toast);
        connect(m_projectSync.get(), &ProjectSync::onlineChanged, this, &AppController::onlineChanged);
        connect(m_projectSync.get(), &ProjectSync::pendingChangesChanged, this,
                &AppController::pendingChangesChanged);
        connect(m_projectSync.get(), &ProjectSync::remoteProjectUpdated, this,
                [this](const QString& id) {
                    reloadProjects();
                    if (!m_hasCurrent || m_current.id != id.toStdString())
                        return;
                    // Rafraîchir en place — ne PAS openProject (émet editorOpened
                    // → stack.push en boucle à chaque echo sync).
                    if (auto p = m_db->getProject(id.toStdString())) {
                        const auto before = m_playChecksSnapshot;
                        m_current = *p;
                        clearGridsDirtyFlag();
                        ++m_gridsRevision;
                        const auto after = m_db->getAllPlayChecks(m_current.id);
                        // Priorité aux overlays du pair (mêmes noms partout).
                        QVariantList remoteOverlays =
                            m_projectSync->takeInboundPlayOverlays(id);
                        const bool skipRecompute =
                            m_projectSync->takeSkipOverlayRecompute(id);
                        if (remoteOverlays.isEmpty() && !skipRecompute) {
                            // Anciens clients sans playOverlays dans le snapshot.
                            remoteOverlays =
                                play::overlaysForNewlyCheckedCells(m_current, before, after);
                        }

                        QVariantList gageOverlays;
                        QVariantList newWinners;
                        QVariantList winBoard;
                        for (const QVariant& v : remoteOverlays) {
                            const QVariantMap ov = v.toMap();
                            if (ov.value(QStringLiteral("kind")).toString()
                                == QLatin1String("winner")) {
                                newWinners = ov.value(QStringLiteral("winners")).toList();
                                winBoard = ov.value(QStringLiteral("scoreboard")).toList();
                            } else {
                                gageOverlays.append(v);
                            }
                        }
                        // Fallback : détecter les grilles nouvellement pleines
                        // (clients sans overlay winner, ou echo partiel).
                        if (newWinners.isEmpty() && !skipRecompute) {
                            const auto wasFull = play::fullPlayersFromChecksMap(m_current, before);
                            const auto board = play::buildScoreboard(m_current, m_db.get());
                            for (const QVariant& rowV : board) {
                                const auto m = rowV.toMap();
                                if (!m.value(QStringLiteral("full")).toBool())
                                    continue;
                                const QString pname =
                                    m.value(QStringLiteral("player")).toString();
                                if (!wasFull.contains(pname))
                                    newWinners.append(m);
                            }
                            if (!newWinners.isEmpty())
                                winBoard = board;
                        }

                        m_playChecksSnapshot = after;
                        emit currentProjectChanged();
                        emit gridsChanged();
                        emit playChecksChanged();
                        if (!newWinners.isEmpty())
                            announceWinners(newWinners, winBoard);
                        if (!gageOverlays.isEmpty())
                            emit playOverlaysTriggered(gageOverlays);
                    }
                });
        m_updater->check();

        const bool active = QGuiApplication::applicationState() == Qt::ApplicationActive;
        m_projectSync->setAppInForeground(active);
        m_projectSync->setDeferBackgroundNotificationsToPush(pushEnabled());
        if (active) {
            m_pushLifecycleReady = true;
            platformConfigurePush(QString(), {}, QString());
        }
    }

    reloadProjects();

    if (screenshotMode)
        return true;

    // Premier lancement : aucun projet → démo jouable avec grilles déjà générées.
    if (m_projectModel->rowCount() == 0) {
        const QString demoId = seedDemoProject();
        if (!demoId.isEmpty()) {
            m_lastTab = 5; // Play
            m_db->setSetting("last_tab", "5");
            openProject(demoId);
            emit toast(QStringLiteral("Projet démo chargé — coche des cases dans Partie !"));
            return true;
        }
    }

    // Migration onglets éditeur.
    // v6 : insertion de « Gages » après Phrases (indices ≥ 2 décalés).
    // v7 : append « Scores » (pas de décalage).
    const auto tabsVer = m_db->getSetting("editor_tabs_v");
    if (!tabsVer || (*tabsVer != "6" && *tabsVer != "7")) {
        if (auto lastTab = m_db->getSetting("last_tab")) {
            int t = QString::fromStdString(*lastTab).toInt();
            if (t >= 2)
                ++t; // Grilles/Impression/Play décalés
            m_lastTab = t;
            m_db->setSetting("last_tab", std::to_string(t));
        }
        m_db->setSetting("editor_tabs_v", "7");
    } else {
        if (*tabsVer != "7")
            m_db->setSetting("editor_tabs_v", "7");
        if (auto lastTab = m_db->getSetting("last_tab"))
            m_lastTab = qBound(0, QString::fromStdString(*lastTab).toInt(), 6);
    }

    const auto currentId = m_db->getSetting("current_project_id");
    if (currentId && openProject(QString::fromStdString(*currentId)))
        return true;

    if (m_projectModel->rowCount() > 0)
        openProject(m_projectModel->idAt(0));

    return true;
}

bool AppController::online() const { return m_projectSync && m_projectSync->online(); }
int AppController::pendingChanges() const
{
    return m_projectSync ? m_projectSync->pendingChanges() : 0;
}

core::Project* AppController::current() { return m_hasCurrent ? &m_current : nullptr; }
const core::Project* AppController::current() const { return m_hasCurrent ? &m_current : nullptr; }

void AppController::touchProject()
{
    if (!m_hasCurrent)
        return;
    m_current.updatedAt = QDateTime::currentMSecsSinceEpoch();
}

void AppController::publishPlayChecksIfShared()
{
    if (!m_hasCurrent || !m_db || !m_projectSync)
        return;
    if (!m_db->getSyncKey(m_current.id))
        return;
    touchProject();
    m_db->upsertProject(m_current);
    m_projectSync->onLocalProjectChange(QString::fromStdString(m_current.id));
}

void AppController::persistCurrent()
{
    if (!m_hasCurrent || !m_db)
        return;
    // Stub d'invitation encore en mémoire alors que le snapshot distant est déjà en DB :
    // ne pas l'écraser (sinon l'invité revoit un projet vide après la sync).
    const bool memEmpty = m_current.cases.empty() && m_current.grids.empty()
                          && m_current.players.empty();
    if (memEmpty) {
        if (auto dbp = m_db->getProject(m_current.id)) {
            const bool dbFull = !dbp->cases.empty() || !dbp->grids.empty()
                                || !dbp->players.empty();
            if (dbFull) {
                m_current = *dbp;
                emit currentProjectChanged();
                ++m_gridsRevision;
                emit gridsChanged();
                return;
            }
        }
        // Stub partagé : pas d'horodatage ni de publish Nostr.
        if (m_db->getSyncKey(m_current.id)) {
            m_db->upsertProject(m_current);
            m_db->setSetting("current_project_id", m_current.id);
            reloadProjects();
            return;
        }
    }
    touchProject();
    m_db->upsertProject(m_current);
    m_db->setSetting("current_project_id", m_current.id);
    if (m_projectSync)
        m_projectSync->onLocalProjectChange(QString::fromStdString(m_current.id));
    reloadProjects();
}

void AppController::markGridsDirty()
{
    if (m_hasCurrent && !m_current.grids.empty() && !m_gridsDirty) {
        m_gridsDirty = true;
        emit gridsDirtyChanged();
    }
}

void AppController::clearGridsDirtyFlag()
{
    if (m_gridsDirty) {
        m_gridsDirty = false;
        emit gridsDirtyChanged();
    }
}

QString AppController::currentProjectId() const
{
    return m_hasCurrent ? QString::fromStdString(m_current.id) : QString();
}

QString AppController::title() const
{
    return m_hasCurrent ? QString::fromStdString(m_current.title) : QString();
}

QString AppController::description() const
{
    return m_hasCurrent ? QString::fromStdString(m_current.description) : QString();
}

int AppController::gridSize() const { return m_hasCurrent ? m_current.gridSize : 5; }
int AppController::startHP() const { return m_hasCurrent ? m_current.startHP : 20; }
bool AppController::freeCenter() const { return !m_hasCurrent || m_current.freeCenter; }
bool AppController::gageMode() const { return m_hasCurrent && m_current.gageMode; }

void AppController::setTitle(const QString& v)
{
    if (!m_hasCurrent)
        return;
    const std::string next = v.toStdString();
    if (m_current.title == next)
        return;
    m_current.title = next;
    touchProject();
    emit currentProjectChanged();
    scheduleAutoSave();
}

void AppController::setDescription(const QString& v)
{
    if (!m_hasCurrent)
        return;
    m_current.description = v.toStdString();
    touchProject();
    emit currentProjectChanged();
    scheduleAutoSave();
}

int AppController::gridRows() const { return m_hasCurrent ? m_current.gridRows : 5; }
int AppController::gridCols() const { return m_hasCurrent ? m_current.gridCols : 5; }

void AppController::setGridSize(int v)
{
    if (!m_hasCurrent)
        return;
    const int n = qBound(2, v, 12);
    m_current.gridRows = n;
    m_current.gridCols = n;
    m_current.gridSize = n;
    markGridsDirty();
    emit currentProjectChanged();
    scheduleAutoSave();
}

void AppController::setGridRows(int v)
{
    if (!m_hasCurrent)
        return;
    m_current.gridRows = qBound(2, v, 12);
    core::normalizeGridDims(m_current);
    markGridsDirty();
    emit currentProjectChanged();
    scheduleAutoSave();
}

void AppController::setGridCols(int v)
{
    if (!m_hasCurrent)
        return;
    m_current.gridCols = qBound(2, v, 12);
    core::normalizeGridDims(m_current);
    markGridsDirty();
    emit currentProjectChanged();
    scheduleAutoSave();
}

void AppController::setStartHP(int v)
{
    if (!m_hasCurrent)
        return;
    m_current.startHP = qBound(1, v, 100);
    emit currentProjectChanged();
    scheduleAutoSave();
}

void AppController::setFreeCenter(bool v)
{
    if (!m_hasCurrent)
        return;
    m_current.freeCenter = v;
    emit currentProjectChanged();
    scheduleAutoSave();
}

void AppController::setGageMode(bool v)
{
    if (!m_hasCurrent)
        return;
    m_current.gageMode = v;
    emit currentProjectChanged();
    scheduleAutoSave();
}

void AppController::setLastTab(int v)
{
    m_lastTab = qBound(0, v, 6);
    if (m_db)
        m_db->setSetting("last_tab", QString::number(m_lastTab).toStdString());
    emit lastTabChanged();
}

QVariantList AppController::players() const
{
    QVariantList out;
    if (!m_hasCurrent)
        return out;
    for (const auto& p : m_current.players)
        out.append(QVariantMap{ { QStringLiteral("name"), QString::fromStdString(p.name) } });
    return out;
}

QVariantList AppController::cases() const
{
    QVariantList out;
    if (!m_hasCurrent)
        return out;
    for (const auto& c : m_current.cases)
        out.append(QVariantMap{
            { QStringLiteral("label"), QString::fromStdString(c.label) },
            { QStringLiteral("points"), c.points },
            { QStringLiteral("rate"), c.rate },
        });
    return out;
}

QVariantList AppController::gages() const
{
    QVariantList out;
    if (!m_hasCurrent)
        return out;
    for (const auto& g : m_current.gages)
        out.append(QVariantMap{
            { QStringLiteral("description"), QString::fromStdString(g.description) },
            { QStringLiteral("hp"), g.hp },
            { QStringLiteral("number"), g.number },
            { QStringLiteral("rate"), g.rate },
        });
    return out;
}

QVariantList AppController::gridsToVariant() const
{
    QVariantList out;
    if (!m_hasCurrent)
        return out;
    for (const auto& grid : m_current.grids) {
        QVariantList rows;
        for (const auto& row : grid.cells) {
            QVariantList cells;
            for (const auto& cell : row) {
                QVariantMap m;
                m.insert(QStringLiteral("label"), QString::fromStdString(cell.label));
                m.insert(QStringLiteral("points"), cell.points);
                m.insert(QStringLiteral("gage"), QString::fromStdString(cell.gage));
                m.insert(QStringLiteral("gageHP"), cell.gageHP);
                m.insert(QStringLiteral("isFree"), cell.isFree);
                cells.append(m);
            }
            // QVariantList::append(QVariantList) aplatit — il faut encapsuler.
            rows.append(QVariant(cells));
        }
        out.append(QVariantMap{
            { QStringLiteral("player"), QString::fromStdString(grid.player) },
            { QStringLiteral("cells"), rows },
        });
    }
    return out;
}

QVariantList AppController::grids() const { return gridsToVariant(); }

void AppController::notify(const QString& message)
{
    emit toast(message);
}

QVariantMap AppController::multipliers() const
{
    return m_hasCurrent ? multToMap(m_current.multipliers) : QVariantMap();
}
QVariantMap AppController::comboGages() const
{
    return m_hasCurrent ? comboToMap(m_current.comboGages) : QVariantMap();
}

int AppController::availableCells() const
{
    return m_hasCurrent ? core::calcRequirements(m_current).available : 0;
}

int AppController::minCases() const { return availableCells(); }

void AppController::reloadProjects()
{
    if (!m_db)
        return;
    QSet<QString> shared;
    for (const auto& id : m_db->sharedProjectIds())
        shared.insert(QString::fromStdString(id));
    m_projectModel->setSharedIds(shared);
    m_projectModel->setProjects(m_db->getAllProjects());
}

QString AppController::createProject()
{
    // Projet vide = pas de bingo. On démarre avec des phrases + grilles générées.
    const QString id = seedDemoProject();
    if (id.isEmpty())
        return {};
    // Personnaliser le titre pour ne pas confondre avec la démo d'accueil.
    if (openProject(id)) {
        m_current.title = "Nouveau Bingo";
        m_current.description = "Modifiez les phrases, puis régénérez les grilles.";
        persistCurrent();
        emit currentProjectChanged();
        m_lastTab = 0; // Réglages
        m_db->setSetting("last_tab", "0");
        emit lastTabChanged();
        emit toast(QStringLiteral("Projet créé — ajuste les réglages, puis les phrases."));
    }
    return id;
}

bool AppController::openProject(const QString& id, bool toPlay)
{
    const auto p = m_db->getProject(id.toStdString());
    if (!p)
        return false;
    m_current = *p;
    core::normalizeGridDims(m_current);
    m_hasCurrent = true;
    clearGridsDirtyFlag();
    m_db->setSetting("current_project_id", m_current.id);
    if (toPlay) {
        m_lastTab = 5; // Play
        m_db->setSetting("last_tab", "5");
        emit lastTabChanged();
    }
    emit currentProjectChanged();
    emit editorOpened(id);
    rememberPlayChecksSnapshot();
    return true;
}

QString AppController::cloneProject(const QString& id)
{
    const auto p = m_db->getProject(id.toStdString());
    if (!p)
        return {};
    auto clone = *p;
    clone.id = core::JsonCodec::makeId();
    clone.title += " (copie)";
    // Garder phrases, gages, grilles — nouvelle partie locale (pas de sync / coches).
    clone.createdAt = QDateTime::currentMSecsSinceEpoch();
    clone.updatedAt = clone.createdAt;
    m_db->upsertProject(clone);
    reloadProjects();
    const QString newId = QString::fromStdString(clone.id);
    if (openProject(newId)) {
        m_lastTab = 0; // Réglages
        m_db->setSetting("last_tab", "0");
        emit lastTabChanged();
        emit toast(QStringLiteral("Projet dupliqué"));
    }
    return newId;
}

void AppController::deleteProject(const QString& id)
{
    m_db->deleteProject(id.toStdString());
    if (m_hasCurrent && m_current.id == id.toStdString()) {
        m_hasCurrent = false;
        emit currentProjectChanged();
    }
    reloadProjects();
}

void AppController::updateProjectMeta(const QString& id, const QString& title,
                                      const QString& description)
{
    auto p = m_db->getProject(id.toStdString());
    if (!p)
        return;
    if (!title.isEmpty())
        p->title = title.trimmed().toStdString();
    p->description = description.toStdString();
    p->updatedAt = QDateTime::currentMSecsSinceEpoch();
    m_db->upsertProject(*p);
    if (m_hasCurrent && m_current.id == id.toStdString()) {
        m_current.title = p->title;
        m_current.description = p->description;
        emit currentProjectChanged();
    }
    reloadProjects();
}

void AppController::saveCurrentProject()
{
    persistCurrent();
    emit toast(QStringLiteral("Enregistré"));
}

void AppController::saveConfig()
{
    if (!m_hasCurrent)
        return;
    // Ne plus laisser l'utilisateur sans grilles : on régénère si possible.
    if (!m_current.cases.empty() && !m_current.players.empty()) {
        const auto result = core::generateAll(m_current);
        if (result.error) {
            m_current.grids.clear();
            emit toast(QString::fromStdString(result.message));
        } else {
            clearGridsDirtyFlag();
            emit toast(QStringLiteral("Configuration enregistrée — grilles régénérées."));
        }
    } else {
        m_current.grids.clear();
        clearGridsDirtyFlag();
        emit toast(QStringLiteral("Configuration enregistrée — ajoutez des phrases pour générer."));
    }
    persistCurrent();
    ++m_gridsRevision;
    emit gridsChanged();
    emit currentProjectChanged();
}

void AppController::scheduleAutoSave()
{
    // Ne pas autosauvegarder un stub d'invitation vide (horodate + course avec le snapshot).
    if (m_hasCurrent && m_db && m_db->getSyncKey(m_current.id)
        && m_current.cases.empty() && m_current.grids.empty()
        && m_current.players.empty())
        return;
    m_autoSaveTimer.start();
}

void AppController::setPlayerName(int index, const QString& name)
{
    if (!m_hasCurrent || index < 0 || index >= static_cast<int>(m_current.players.size()))
        return;
    m_current.players[static_cast<size_t>(index)].name = name.toStdString();
    emit currentProjectChanged();
    scheduleAutoSave();
}

void AppController::addPlayer()
{
    if (!m_hasCurrent)
        return;
    m_current.players.push_back({ "Joueur " + std::to_string(m_current.players.size() + 1) });
    emit currentProjectChanged();
    scheduleAutoSave();
}

void AppController::removePlayer(int index)
{
    if (!m_hasCurrent || index < 0 || index >= static_cast<int>(m_current.players.size()))
        return;
    m_current.players.erase(m_current.players.begin() + index);
    emit currentProjectChanged();
    scheduleAutoSave();
}

void AppController::addCase(const QString& label, int points, int rate)
{
    if (!m_hasCurrent)
        return;
    m_current.cases.push_back({ label.toStdString(), points, rate });
    markGridsDirty();
    emit currentProjectChanged();
    scheduleAutoSave();
}

void AppController::updateCase(int index, const QString& label, int points, int rate)
{
    if (!m_hasCurrent || index < 0 || index >= static_cast<int>(m_current.cases.size()))
        return;
    auto& c = m_current.cases[static_cast<size_t>(index)];
    c.label = label.toStdString();
    c.points = points;
    c.rate = rate;
    markGridsDirty();
    emit currentProjectChanged();
    scheduleAutoSave();
}

void AppController::removeCase(int index)
{
    if (!m_hasCurrent || index < 0 || index >= static_cast<int>(m_current.cases.size()))
        return;
    m_current.cases.erase(m_current.cases.begin() + index);
    markGridsDirty();
    emit currentProjectChanged();
    scheduleAutoSave();
}

void AppController::addGage(const QString& description, int hp, int number, int rate)
{
    if (!m_hasCurrent)
        return;
    const QString trimmed = description.trimmed();
    if (trimmed.isEmpty()) {
        emit toast(QStringLiteral("Écrivez la description du gage"));
        return;
    }
    core::Gage g;
    g.description = trimmed.toStdString();
    g.hp = qMax(0, hp);
    g.number = qMax(1, number);
    g.rate = qBound(0, rate, 100);
    m_current.gages.push_back(std::move(g));
    emit currentProjectChanged();
    scheduleAutoSave();
}

void AppController::updateGage(int index, const QString& description, int hp,
                               int number, int rate)
{
    if (!m_hasCurrent || index < 0 || index >= static_cast<int>(m_current.gages.size()))
        return;
    const QString trimmed = description.trimmed();
    if (trimmed.isEmpty()) {
        emit toast(QStringLiteral("Écrivez la description du gage"));
        return;
    }
    auto& g = m_current.gages[static_cast<size_t>(index)];
    g.description = trimmed.toStdString();
    g.hp = qMax(0, hp);
    g.number = qMax(1, number);
    g.rate = qBound(0, rate, 100);
    emit currentProjectChanged();
    scheduleAutoSave();
}

void AppController::removeGage(int index)
{
    if (!m_hasCurrent || index < 0 || index >= static_cast<int>(m_current.gages.size()))
        return;
    m_current.gages.erase(m_current.gages.begin() + index);
    emit currentProjectChanged();
    scheduleAutoSave();
}

int AppController::maxGageNumber() const
{
    if (!m_hasCurrent || m_current.gages.empty())
        return 1;
    int m = 1;
    for (const auto& g : m_current.gages)
        m = qMax(m, g.number);
    return m;
}

void AppController::setMultiplier(const QString& key, int value)
{
    if (!m_hasCurrent)
        return;
    if (key == QLatin1String("line")) m_current.multipliers.line = value;
    else if (key == QLatin1String("column")) m_current.multipliers.column = value;
    else if (key == QLatin1String("diagonal")) m_current.multipliers.diagonal = value;
    else if (key == QLatin1String("full")) m_current.multipliers.full = value;
    emit currentProjectChanged();
    scheduleAutoSave();
}

void AppController::setComboGage(const QString& key, const QString& value)
{
    if (!m_hasCurrent)
        return;
    const auto v = value.toStdString();
    if (key == QLatin1String("line")) m_current.comboGages.line = v;
    else if (key == QLatin1String("column")) m_current.comboGages.column = v;
    else if (key == QLatin1String("diagonal")) m_current.comboGages.diagonal = v;
    emit currentProjectChanged();
    scheduleAutoSave();
}

QString AppController::generateAll()
{
    if (!m_hasCurrent) {
        const auto msg = QStringLiteral("Aucun projet ouvert.");
        emit toast(msg);
        return msg;
    }
    const auto result = core::generateAll(m_current);
    if (result.error) {
        const auto msg = QString::fromStdString(result.message);
        emit toast(msg);
        return msg;
    }
    clearGridsDirtyFlag();
    persistCurrent();
    ++m_gridsRevision;
    emit gridsChanged();
    emit currentProjectChanged();
    const auto msg = result.repeats
        ? QStringLiteral("Grilles générées (doublons possibles — pool insuffisant).")
        : QStringLiteral("Grilles générées.");
    emit toast(msg);
    return msg;
}

QString AppController::seedDemoProject()
{
    if (!m_db)
        return {};

    auto p = core::JsonCodec::defaultProject();
    p.title = "Soirée Cinéma (démo)";
    p.description = "Bingo des clichés du film — 4 joueurs, grille 5×5, mode gage "
                    "avec plusieurs gages par n° (tirage pondéré).";
    p.gridRows = 5;
    p.gridCols = 5;
    p.gridSize = 5;
    p.startHP = 20;
    p.freeCenter = true;
    p.gageMode = true;
    p.players = { { "Léa" }, { "Max" }, { "Sam" }, { "Chloé" } };
    // Plusieurs gages peuvent partager un n° ; rate = poids relatif au tirage.
    p.gages = {
        { "Imite la voix du personnage", 0, 1, 70 },
        { "Chante la réplique en mode opéra", 0, 1, 25 },
        { "Bois une gorgée (risqué)", 0, 1, 5 },
        { "Mime la scène en 10 secondes", 0, 2, 80 },
        { "Fais le bruitage live", 0, 2, 20 },
        { "Inventez la réplique suivante", 0, 3, 60 },
        { "Raconte la scène en chuchotant", 0, 3, 30 },
        { "Change de place avec ton voisin", 0, 3, 10 },
        { "Debout 5 secondes", 0, 4, 90 },
        { "Danse 5 secondes (risqué)", 0, 4, 10 },
        { "Compliment ridicule au voisin", 0, 5, 100 },
        { "Titre alternatif du film", 0, 6, 75 },
        { "Pitch la suite en 15 s", 0, 6, 25 },
        { "Choisis un Oscar inventé", 0, 7, 100 },
        { "Imite le méchant", 0, 8, 55 },
        { "Imite le héros", 0, 8, 35 },
        { "Cri de guerre (risqué)", 0, 8, 10 },
    };
    p.comboGages.line = "Toute la table boit une gorgée";
    p.comboGages.column = "Le joueur à ta gauche invente un titre alternatif";
    p.comboGages.diagonal = "Tout le monde se lève 5 secondes";

    // Assez de phrases (> cellules jouables) + taux < 100 pour des grilles vraiment
    // différentes entre joueurs (24 cases utiles sur 5×5 avec FREE).
    static const struct {
        const char* label;
        int rate;
    } phrases[] = {
        { "Le héros se réveille en sursaut", 90 },
        { "Flashback en noir et blanc", 80 },
        { "Le méchant monologue", 95 },
        { "Course-poursuite en voiture", 85 },
        { "Explosion spectaculaire", 90 },
        { "Révélation : un traître", 85 },
        { "Baiser sous la pluie", 70 },
        { "Le mentor meurt", 75 },
        { "Montage entraînement", 80 },
        { "Le chien survit", 70 },
        { "Twist prévisible", 90 },
        { "Bagarre au ralenti", 85 },
        { "Il refuse d'abord la quête", 80 },
        { "Vilain qui tombe", 85 },
        { "Réplique culte répétée", 90 },
        { "Se déguise mal", 75 },
        { "Fin ouverte", 70 },
        { "Caméo surprise", 65 },
        { "Plan produit trop long", 60 },
        { "Ils se séparent puis se retrouvent", 85 },
        { "Vision / fantôme", 70 },
        { "Compte à rebours", 80 },
        { "Sauvetage in extremis", 90 },
        { "Gag après le générique", 55 },
        { "Le vrai méchant était un allié", 80 },
        { "Arme qui s'enraye", 75 },
        { "Héros qui tombe amoureux", 70 },
        { "Quiproquo comique", 80 },
        { "Fuite dans les égouts", 60 },
        { "Saut d'un immeuble", 70 },
        { "Appel téléphonique interrompu", 75 },
        { "Carte / plan étalé sur une table", 80 },
        { "Regarde par la fenêtre, voit l'ennemi", 75 },
        { "Électrocuté mais ça va", 65 },
        { "Voiture qui n'a presque plus d'essence", 70 },
        { "Poursuite à pied dans une foule", 75 },
        { "Amnésie temporaire", 55 },
        { "Coffre-fort / code secret", 70 },
        { "Le copain sacrifie / se sacrifie", 75 },
        { "Réunion d'équipe avant le coup final", 80 },
        { "Pluie de balles, personne n'est touché", 85 },
        { "Héros qui enlève ses lunettes", 60 },
        { "Discours motivant avant l'assaut", 75 },
        { "Le vilain dit « on se reverra »", 85 },
        { "Explosion vue de loin au ralenti", 80 },
        { "Enfant qui sauve la situation", 55 },
        { "Trahison révélée par un regard", 70 },
        { "Musique épique qui monte", 90 },
        { "Héros qui dit non puis accepte", 80 },
        { "Scène de bar / alcool", 65 },
        { "Ordinateur qui « pirate » trop vite", 70 },
        { "Chute dans l'eau", 60 },
        { "Le méchant applaudit lentement", 75 },
        { "Fusillade derrière des caisses", 80 },
        { "Retour à la case départ (lieu du début)", 70 },
    };
    p.cases.clear();
    int maxNum = 1;
    for (const auto& g : p.gages)
        maxNum = std::max(maxNum, g.number);
    int i = 0;
    for (const auto& ph : phrases) {
        const int gageNum = (i % maxNum) + 1;
        p.cases.push_back({ ph.label, gageNum, ph.rate });
        ++i;
    }

    std::mt19937 gen(4242);
    const core::Rng rng = [&gen]() {
        return std::uniform_real_distribution<>(0.0, 100.0)(gen);
    };
    const auto result = core::generateAll(p, rng);
    if (result.error || p.grids.empty()) {
        qWarning("seedDemoProject failed: %s", result.message.c_str());
        return {};
    }

    m_db->upsertProject(p);

    {
        const int rowsN = p.gridRows;
        const int colsN = p.gridCols;
        const int midR = rowsN / 2;
        const int midC = colsN / 2;
        const bool freeCtr = core::projectHasFreeCenter(p);
        QJsonArray rows;
        for (int r = 0; r < rowsN; ++r) {
            QJsonArray row;
            for (int c = 0; c < colsN; ++c) {
                const bool free = freeCtr && r == midR && c == midC;
                row.append(free || (rowsN == colsN && r == c));
            }
            rows.append(row);
        }
        m_db->savePlayChecks(p.id, p.grids[0].player,
                             QJsonDocument(rows).toJson(QJsonDocument::Compact).toStdString());
    }

    reloadProjects();
    return QString::fromStdString(p.id);
}

void AppController::reshuffleGrid(int playerIdx)
{
    if (!m_hasCurrent)
        return;
    core::reshuffleGrid(m_current, playerIdx, core::Rng{});
    // Nouveau tirage → les anciennes coches par position n'ont plus de sens.
    if (m_db && playerIdx >= 0
        && playerIdx < static_cast<int>(m_current.grids.size())) {
        const auto& player = m_current.grids[static_cast<size_t>(playerIdx)].player;
        const auto empty = play::checksToVariant(play::emptyChecks(m_current.gridRows, m_current.gridCols,
                                                     core::projectHasFreeCenter(m_current)));
        m_db->savePlayChecks(m_current.id, player,
                             QJsonDocument(QJsonArray::fromVariantList(empty))
                                 .toJson(QJsonDocument::Compact)
                                 .toStdString());
        emit playChecksChanged();
    }
    persistCurrent();
    ++m_gridsRevision;
    emit gridsChanged();
    emit currentProjectChanged();
}

void AppController::swapGridCells(int playerIdx, int r1, int c1, int r2, int c2)
{
    if (!m_hasCurrent || playerIdx < 0 || playerIdx >= static_cast<int>(m_current.grids.size()))
        return;
    auto& cells = m_current.grids[static_cast<size_t>(playerIdx)].cells;
    if (r1 < 0 || r2 < 0 || c1 < 0 || c2 < 0)
        return;
    if (r1 >= static_cast<int>(cells.size()) || r2 >= static_cast<int>(cells.size()))
        return;
    if (c1 >= static_cast<int>(cells[static_cast<size_t>(r1)].size())
        || c2 >= static_cast<int>(cells[static_cast<size_t>(r2)].size()))
        return;
    std::swap(cells[static_cast<size_t>(r1)][static_cast<size_t>(c1)],
              cells[static_cast<size_t>(r2)][static_cast<size_t>(c2)]);

    // Les coches sont indexées par position : les faire suivre le libellé déplacé,
    // sinon Play (local + sync) affiche la coche sur la mauvaise case.
    if (m_db) {
        const auto& player = m_current.grids[static_cast<size_t>(playerIdx)].player;
        const int rows = m_current.gridRows;
        const int cols = m_current.gridCols;
        auto checks = play::emptyChecks(rows, cols, core::projectHasFreeCenter(m_current));
        if (const auto json = m_db->getPlayChecks(m_current.id, player)) {
            const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(*json));
            checks = play::checksFromVariant(doc.array().toVariantList(), rows, cols);
        }
        if (r1 < rows && r2 < rows && c1 < cols && c2 < cols) {
            const bool tmp = checks[static_cast<size_t>(r1)][static_cast<size_t>(c1)];
            checks[static_cast<size_t>(r1)][static_cast<size_t>(c1)] =
                checks[static_cast<size_t>(r2)][static_cast<size_t>(c2)];
            checks[static_cast<size_t>(r2)][static_cast<size_t>(c2)] = tmp;
            m_db->savePlayChecks(m_current.id, player,
                                 QJsonDocument(QJsonArray::fromVariantList(play::checksToVariant(checks)))
                                     .toJson(QJsonDocument::Compact)
                                     .toStdString());
            emit playChecksChanged();
        }
    }

    persistCurrent();
    ++m_gridsRevision;
    emit gridsChanged();
    emit currentProjectChanged();
}

void AppController::replaceGridCell(int playerIdx, int row, int col, int caseIdx)
{
    if (!m_hasCurrent || playerIdx < 0 || caseIdx < 0
        || caseIdx >= static_cast<int>(m_current.cases.size()))
        return;
    if (playerIdx >= static_cast<int>(m_current.grids.size()))
        return;
    auto& cells = m_current.grids[static_cast<size_t>(playerIdx)].cells;
    if (row < 0 || col < 0 || row >= static_cast<int>(cells.size())
        || col >= static_cast<int>(cells[static_cast<size_t>(row)].size()))
        return;
    const auto& src = m_current.cases[static_cast<size_t>(caseIdx)];
    cells[static_cast<size_t>(row)][static_cast<size_t>(col)] =
        { src.label, src.points, src.rate, "", 0, false };
    persistCurrent();
    ++m_gridsRevision;
    emit gridsChanged();
    emit currentProjectChanged();
}

void AppController::setGridCellLabel(int playerIdx, int row, int col,
                                     const QString& label, int points)
{
    if (!m_hasCurrent || playerIdx < 0
        || playerIdx >= static_cast<int>(m_current.grids.size()))
        return;
    auto& cells = m_current.grids[static_cast<size_t>(playerIdx)].cells;
    if (row < 0 || col < 0 || row >= static_cast<int>(cells.size())
        || col >= static_cast<int>(cells[static_cast<size_t>(row)].size()))
        return;
    auto& cell = cells[static_cast<size_t>(row)][static_cast<size_t>(col)];
    if (cell.isFree)
        return;
    const QString trimmed = label.trimmed();
    if (trimmed.isEmpty())
        return;
    cell.label = trimmed.toStdString();
    if (points >= 0)
        cell.points = points;
    persistCurrent();
    ++m_gridsRevision;
    emit gridsChanged();
    emit currentProjectChanged();
}

void AppController::moveGrid(int fromIdx, int toIdx)
{
    if (!m_hasCurrent)
        return;
    if (fromIdx < 0 || toIdx < 0 || fromIdx >= static_cast<int>(m_current.grids.size())
        || toIdx >= static_cast<int>(m_current.grids.size()))
        return;
    auto g = m_current.grids[static_cast<size_t>(fromIdx)];
    m_current.grids.erase(m_current.grids.begin() + fromIdx);
    m_current.grids.insert(m_current.grids.begin() + toIdx, g);
    persistCurrent();
    ++m_gridsRevision;
    emit gridsChanged();
    emit currentProjectChanged();
}

void AppController::assignGridToPlayer(int gridIdx, const QString& playerName)
{
    if (!m_hasCurrent || !m_db || gridIdx < 0
        || gridIdx >= static_cast<int>(m_current.grids.size()))
        return;
    const QString wanted = playerName.trimmed();
    if (wanted.isEmpty())
        return;

    auto& grid = m_current.grids[static_cast<size_t>(gridIdx)];
    const QString previous = QString::fromStdString(grid.player);
    if (previous == wanted)
        return;

    int otherIdx = -1;
    for (int i = 0; i < static_cast<int>(m_current.grids.size()); ++i) {
        if (i == gridIdx)
            continue;
        if (QString::fromStdString(m_current.grids[static_cast<size_t>(i)].player) == wanted) {
            otherIdx = i;
            break;
        }
    }

    const std::string projectId = m_current.id;
    auto checksPrev = m_db->getPlayChecks(projectId, previous.toStdString());
    auto checksWanted = m_db->getPlayChecks(projectId, wanted.toStdString());

    if (otherIdx >= 0) {
        // Échange des libellés : chaque feuille garde ses cases, les noms basculent.
        m_current.grids[static_cast<size_t>(otherIdx)].player = previous.toStdString();
        grid.player = wanted.toStdString();
        // Les cochages suivent la feuille (contenu), pas le nom : on échange les blobs.
        if (checksPrev)
            m_db->savePlayChecks(projectId, wanted.toStdString(), *checksPrev);
        else
            m_db->savePlayChecks(projectId, wanted.toStdString(), "[]");
        if (checksWanted)
            m_db->savePlayChecks(projectId, previous.toStdString(), *checksWanted);
        else
            m_db->savePlayChecks(projectId, previous.toStdString(), "[]");
    } else {
        grid.player = wanted.toStdString();
        if (checksPrev)
            m_db->savePlayChecks(projectId, wanted.toStdString(), *checksPrev);
        m_db->savePlayChecks(projectId, previous.toStdString(), "[]");
    }

    persistCurrent();
    ++m_gridsRevision;
    emit gridsChanged();
    emit currentProjectChanged();
    emit toast(QStringLiteral("Grille assignée à %1").arg(wanted));
}

static bool writeFile(const QString& path, const std::string& content)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return f.write(QByteArray::fromStdString(content)) >= 0;
}

bool AppController::exportCurrentJson(const QString& filePath)
{
    if (!m_hasCurrent || !m_db)
        return false;
    core::ProjectBundle b;
    b.project = m_current;
    b.playChecks = m_db->getAllPlayChecks(m_current.id);
    b.hasPlayChecks = true;
    return writeFile(filePath, core::JsonCodec::projectBundleToJson(b));
}

bool AppController::importJsonFile(const QString& filePath)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    bool ok = false;
    auto bundle = core::JsonCodec::projectBundleFromJson(f.readAll().toStdString(), &ok);
    if (!ok)
        return false;
    m_db->upsertProject(bundle.project);
    if (bundle.hasPlayChecks)
        m_db->replaceAllPlayChecks(bundle.project.id, bundle.playChecks);
    reloadProjects();
    openProject(QString::fromStdString(bundle.project.id));
    return true;
}

bool AppController::exportProjectJson(const QString& id, const QString& filePath)
{
    const auto p = m_db->getProject(id.toStdString());
    if (!p)
        return false;
    core::ProjectBundle b;
    b.project = *p;
    b.playChecks = m_db->getAllPlayChecks(p->id);
    b.hasPlayChecks = true;
    return writeFile(filePath, core::JsonCodec::projectBundleToJson(b));
}

bool AppController::exportAllJson(const QString& filePath)
{
    std::vector<core::ProjectBundle> bundles;
    for (const auto& p : m_db->getAllProjects()) {
        core::ProjectBundle b;
        b.project = p;
        b.playChecks = m_db->getAllPlayChecks(p.id);
        b.hasPlayChecks = true;
        bundles.push_back(std::move(b));
    }
    return writeFile(filePath, core::JsonCodec::exportAllBundles(bundles));
}

int AppController::importAllJsonFile(const QString& filePath)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly))
        return -1;
    bool ok = false;
    const auto bundles = core::JsonCodec::importAllBundles(f.readAll().toStdString(), &ok);
    if (!ok)
        return -1;
    for (const auto& b : bundles) {
        m_db->upsertProject(b.project);
        if (b.hasPlayChecks)
            m_db->replaceAllPlayChecks(b.project.id, b.playChecks);
    }
    reloadProjects();
    return static_cast<int>(bundles.size());
}

namespace {

constexpr auto kJsonFilter = "JSON (*.json)";
constexpr auto kCsvFilter  = "CSV (*.csv)";

QString ensureJsonSuffix(QString path)
{
    if (!path.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive))
        path += QStringLiteral(".json");
    return path;
}

QString ensureCsvSuffix(QString path)
{
    if (!path.endsWith(QStringLiteral(".csv"), Qt::CaseInsensitive))
        path += QStringLiteral(".csv");
    return path;
}

QStringList csvErrorsToList(const std::vector<std::string>& errors)
{
    QStringList out;
    out.reserve(static_cast<int>(errors.size()));
    for (const auto& e : errors)
        out << QString::fromStdString(e);
    return out;
}

QVariantMap csvImportToMap(const core::CsvImportResult& r)
{
    // ok si aucune erreur, ou au moins une ligne importée (succès partiel).
    const bool ok = r.errors.empty() || r.added > 0;
    return QVariantMap{
        {QStringLiteral("ok"), ok},
        {QStringLiteral("added"), r.added},
        {QStringLiteral("skipped"), r.skipped},
        {QStringLiteral("errors"), csvErrorsToList(r.errors)},
    };
}

} // namespace

void AppController::pickExportCurrentJson()
{
    const QString path = ensureJsonSuffix(QFileDialog::getSaveFileName(
        nullptr, tr("Exporter le projet"), QStringLiteral("bingo-export.json"), kJsonFilter));
    if (path.isEmpty())
        return;
    if (exportCurrentJson(path))
        emit toast(tr("Projet exporté"));
    else
        emit toast(tr("Export impossible"));
}

void AppController::pickExportProjectJson()
{
    const QString path = ensureJsonSuffix(QFileDialog::getSaveFileName(
        nullptr, tr("Exporter le projet"), QStringLiteral("bingo-projet.json"), kJsonFilter));
    if (path.isEmpty())
        return;
    if (exportProjectJson(currentProjectId(), path))
        emit toast(tr("Projet exporté"));
    else
        emit toast(tr("Export impossible"));
}

void AppController::pickExportAllJson()
{
    const QString path = ensureJsonSuffix(QFileDialog::getSaveFileName(
        nullptr, tr("Exporter tous les projets"),
        QStringLiteral("bingo-tous-projets.json"), kJsonFilter));
    if (path.isEmpty())
        return;
    if (exportAllJson(path))
        emit toast(tr("Projets exportés"));
    else
        emit toast(tr("Export impossible"));
}

void AppController::pickImportJson()
{
    const QString path = QFileDialog::getOpenFileName(
        nullptr, tr("Importer un projet"), {}, kJsonFilter);
    if (path.isEmpty())
        return;
    if (importJsonFile(path))
        emit toast(tr("Projet importé"));
    else
        emit toast(tr("Import impossible"));
}

void AppController::pickImportAllJson()
{
    const QString path = QFileDialog::getOpenFileName(
        nullptr, tr("Importer des projets"), {}, kJsonFilter);
    if (path.isEmpty())
        return;
    const int n = importAllJsonFile(path);
    if (n >= 0)
        emit toast(tr("%1 projet(s) importé(s)").arg(n));
    else
        emit toast(tr("Import impossible"));
}

QVariantMap AppController::importPhrasesCsvText(const QString& text, bool replace)
{
    if (!m_hasCurrent) {
        return QVariantMap{
            {QStringLiteral("ok"), false},
            {QStringLiteral("added"), 0},
            {QStringLiteral("skipped"), 0},
            {QStringLiteral("errors"), QStringList{tr("Aucun projet ouvert")}},
        };
    }
    const auto r = core::importPhrasesCsv(m_current, text.toStdString(), replace);
    markGridsDirty();
    emit currentProjectChanged();
    scheduleAutoSave();
    return csvImportToMap(r);
}

QVariantMap AppController::importGagesCsvText(const QString& text, bool replace)
{
    if (!m_hasCurrent) {
        return QVariantMap{
            {QStringLiteral("ok"), false},
            {QStringLiteral("added"), 0},
            {QStringLiteral("skipped"), 0},
            {QStringLiteral("errors"), QStringList{tr("Aucun projet ouvert")}},
        };
    }
    const auto r = core::importGagesCsv(m_current, text.toStdString(), replace);
    markGridsDirty();
    emit currentProjectChanged();
    scheduleAutoSave();
    return csvImportToMap(r);
}

QString AppController::exportPhrasesCsvText() const
{
    if (!m_hasCurrent)
        return {};
    return QString::fromStdString(core::exportPhrasesCsv(m_current));
}

QString AppController::exportGagesCsvText() const
{
    if (!m_hasCurrent)
        return {};
    return QString::fromStdString(core::exportGagesCsv(m_current));
}

void AppController::pickImportPhrasesCsv(bool replace)
{
    if (!m_hasCurrent) {
        emit toast(tr("Aucun projet ouvert"));
        return;
    }
    const QString path = QFileDialog::getOpenFileName(
        nullptr, tr("Importer des phrases (CSV)"), {}, kCsvFilter);
    if (path.isEmpty())
        return;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        emit toast(tr("Impossible de lire le fichier"));
        return;
    }
    const auto result = importPhrasesCsvText(QString::fromUtf8(f.readAll()), replace);
    const int added = result.value(QStringLiteral("added")).toInt();
    const QStringList errs = result.value(QStringLiteral("errors")).toStringList();
    if (errs.isEmpty())
        emit toast(tr("%1 phrase(s) ajoutée(s)").arg(added));
    else if (added > 0)
        emit toast(tr("%1 phrase(s) ajoutée(s), %2 erreur(s)").arg(added).arg(errs.size()));
    else
        emit toast(errs.isEmpty() ? tr("Import phrases impossible") : errs.first());
}

void AppController::pickImportGagesCsv(bool replace)
{
    if (!m_hasCurrent) {
        emit toast(tr("Aucun projet ouvert"));
        return;
    }
    const QString path = QFileDialog::getOpenFileName(
        nullptr, tr("Importer des gages (CSV)"), {}, kCsvFilter);
    if (path.isEmpty())
        return;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        emit toast(tr("Impossible de lire le fichier"));
        return;
    }
    const auto result = importGagesCsvText(QString::fromUtf8(f.readAll()), replace);
    const int added = result.value(QStringLiteral("added")).toInt();
    const QStringList errs = result.value(QStringLiteral("errors")).toStringList();
    if (errs.isEmpty())
        emit toast(tr("%1 gage(s) ajouté(s)").arg(added));
    else if (added > 0)
        emit toast(tr("%1 gage(s) ajouté(s), %2 erreur(s)").arg(added).arg(errs.size()));
    else
        emit toast(errs.isEmpty() ? tr("Import gages impossible") : errs.first());
}

void AppController::pickExportPhrasesCsv()
{
    if (!m_hasCurrent) {
        emit toast(tr("Aucun projet ouvert"));
        return;
    }
    const QString path = ensureCsvSuffix(QFileDialog::getSaveFileName(
        nullptr, tr("Exporter les phrases (CSV)"), QStringLiteral("phrases.csv"), kCsvFilter));
    if (path.isEmpty())
        return;
    if (writeFile(path, exportPhrasesCsvText().toStdString()))
        emit toast(tr("Phrases exportées"));
    else
        emit toast(tr("Export impossible"));
}

void AppController::pickExportGagesCsv()
{
    if (!m_hasCurrent) {
        emit toast(tr("Aucun projet ouvert"));
        return;
    }
    const QString path = ensureCsvSuffix(QFileDialog::getSaveFileName(
        nullptr, tr("Exporter les gages (CSV)"), QStringLiteral("gages.csv"), kCsvFilter));
    if (path.isEmpty())
        return;
    if (writeFile(path, exportGagesCsvText().toStdString()))
        emit toast(tr("Gages exportés"));
    else
        emit toast(tr("Export impossible"));
}

QString AppController::buildShareUrl()
{
    if (!m_hasCurrent || !m_projectSync)
        return {};
    // Flush mémoire → SQLite avant publish Nostr, sinon l'invité reçoit une version périmée.
    if (m_autoSaveTimer.isActive())
        m_autoSaveTimer.stop();
    persistCurrent();
    return m_projectSync->joinUri(currentProjectId(), title());
}

QString AppController::joinUriForProject(const QString& projectId)
{
    if (!m_db || !m_projectSync || projectId.isEmpty())
        return {};
    if (m_hasCurrent && m_current.id == projectId.toStdString()) {
        if (m_autoSaveTimer.isActive())
            m_autoSaveTimer.stop();
        persistCurrent();
    }
    const auto p = m_db->getProject(projectId.toStdString());
    if (!p)
        return {};
    return m_projectSync->joinUri(projectId, QString::fromStdString(p->title));
}

bool AppController::importSharePayload(const QString& payload)
{
    bool ok = false;
    auto p = core::ShareCodec::parseSharePayload(payload.toStdString(), &ok);
    if (!ok)
        return false;
    m_db->upsertProject(p);
    reloadProjects();
    openProject(QString::fromStdString(p.id));
    return true;
}

namespace {

bool isValidRelayUrl(const QUrl &url)
{
    if (!url.isValid() || url.host().isEmpty())
        return false;
    const QString scheme = url.scheme().toLower();
    return scheme == QLatin1String("wss") || scheme == QLatin1String("ws");
}

QStringList parseRelayUrlText(const QString &text)
{
    QStringList out;
    for (const QString &part :
         text.split(QRegularExpression(QStringLiteral("[\\n,]+")), Qt::SkipEmptyParts)) {
        const QString trimmed = part.trimmed();
        if (!trimmed.isEmpty())
            out.append(trimmed);
    }
    return out;
}

} // namespace

QString AppController::relayUrls() const
{
    if (!m_db)
        return defaultRelayUrls();
    const auto v = m_db->getSetting("relays");
    if (v && !v->empty())
        return QString::fromStdString(*v).replace(QLatin1Char(','), QLatin1Char('\n'));
    return defaultRelayUrls();
}

QString AppController::defaultRelayUrls() const
{
    QStringList parts;
    for (const QUrl &u : net::RelayPool::defaultRelays())
        parts.append(u.toString());
    return parts.join(QLatin1Char('\n'));
}

void AppController::setRelayUrls(const QString &text)
{
    if (!m_db || !m_relayPool || !m_projectSync)
        return;

    QList<QUrl> urls;
    for (const QString &line : parseRelayUrlText(text)) {
        const QUrl url(line);
        if (!isValidRelayUrl(url)) {
            emit toast(QStringLiteral("URL de relais invalide : %1").arg(line));
            return;
        }
        urls.append(url);
    }
    if (urls.isEmpty()) {
        emit toast(QStringLiteral("Indiquez au moins un relais wss://"));
        return;
    }

    QStringList stored;
    for (const QUrl &u : urls)
        stored.append(u.toString());
    m_db->setSetting("relays", stored.join(QLatin1Char(',')).toStdString());

    m_relayPool->setRelays(urls);
    m_relayPool->connectAll();
    m_projectSync->subscribeAll(0);
    m_projectSync->catchUpOnForeground();

    emit relayUrlsChanged();
    emit toast(urls.size() > 1
                   ? QStringLiteral("Relais mis à jour (%1)").arg(urls.size())
                   : QStringLiteral("Relais mis à jour"));
}

void AppController::resetRelayUrls()
{
    setRelayUrls(defaultRelayUrls());
}

bool AppController::pushEnabled() const
{
    if (!m_db)
        return true;
    const auto v = m_db->getSetting("pushEnabled");
    return !v || *v != "0";
}

QString AppController::pushBaseUrl() const
{
    if (!m_db)
        return defaultPushBaseUrl();
    const auto v = m_db->getSetting("pushBaseUrl");
    if (v && !v->empty())
        return QString::fromStdString(*v);
    return defaultPushBaseUrl();
}

QString AppController::defaultPushBaseUrl() const
{
    return QStringLiteral("https://colo-apps.les-crevettes-cevenoles.fr/ntfy");
}

void AppController::setPushSettings(bool enabled, const QString &baseUrl)
{
    if (!m_db)
        return;

    const QString trimmed = baseUrl.trimmed();
    if (enabled && !trimmed.isEmpty()) {
        const QUrl u(trimmed);
        if (!u.isValid() || u.scheme().isEmpty()) {
            emit toast(QStringLiteral("URL push invalide"));
            return;
        }
    }

    m_db->setSetting("pushEnabled", enabled ? "1" : "0");
    if (!trimmed.isEmpty())
        m_db->setSetting("pushBaseUrl", trimmed.toStdString());

    refreshPushTopics();
    emit pushSettingsChanged();
    emit toast(enabled ? QStringLiteral("Notifications push activées")
                       : QStringLiteral("Notifications push désactivées"));
}

void AppController::refreshPushTopics()
{
    if (!m_db || !m_projectSync)
        return;

    const bool pushOn = pushEnabled();
    m_projectSync->setDeferBackgroundNotificationsToPush(pushOn);

    if (!pushOn) {
        platformConfigurePush(QString(), {}, QString());
        return;
    }

    if (QGuiApplication::applicationState() == Qt::ApplicationActive) {
        platformConfigurePush(QString(), {}, QString());
        return;
    }

    QStringList topics;
    for (const auto &projectId : m_db->sharedProjectIds()) {
        const auto key = m_db->getSyncKey(projectId);
        if (!key)
            continue;
        topics.append(net::pushTopicForChannel(
            QString::fromStdString(net::deriveChannelTag(*key))));
    }
    platformConfigurePush(pushBaseUrl(), topics, m_deviceId);
}

void AppController::onApplicationStateChanged(Qt::ApplicationState state)
{
    if (!m_projectSync)
        return;

    const bool active = (state == Qt::ApplicationActive);
    m_projectSync->setAppInForeground(active);
    m_projectSync->setDeferBackgroundNotificationsToPush(pushEnabled());

    if (active) {
        m_pushLifecycleReady = true;
        platformConfigurePush(QString(), {}, QString());
        m_projectSync->catchUpOnForeground();
        reloadProjects();
    } else if (m_pushLifecycleReady) {
        refreshPushTopics();
    }
}

void AppController::enableProjectSharing()
{
    if (m_hasCurrent && m_projectSync)
        m_projectSync->enableSharing(currentProjectId());
    refreshPushTopics();
}

bool AppController::joinProjectUri(const QString& uri)
{
    if (!m_projectSync)
        return false;
    const QString id = m_projectSync->joinFromUri(uri.trimmed());
    if (id.isEmpty())
        return false;
    reloadProjects();
    openProject(id);
    refreshPushTopics();
    return true;
}

void AppController::handleJoinUrl(const QUrl& url)
{
    if (!joinProjectUri(url.toString(QUrl::FullyEncoded)))
        emit toast(QStringLiteral("Lien d'invitation invalide"));
}

void AppController::leaveProject(const QString& projectId)
{
    if (!m_db || projectId.isEmpty())
        return;
    if (m_projectSync)
        m_projectSync->leaveSharing(projectId);
    m_db->deleteProject(projectId.toStdString());
    if (m_hasCurrent && m_current.id == projectId.toStdString()) {
        m_hasCurrent = false;
        emit currentProjectChanged();
    }
    reloadProjects();
    emit toast(QStringLiteral("Projet retiré de cet appareil"));
    refreshPushTopics();
}

bool AppController::isProjectShared(const QString& projectId) const
{
    if (!m_db || projectId.isEmpty())
        return false;
    return m_db->getSyncKey(projectId.toStdString()).has_value();
}

QVariantList AppController::loadPlayChecks(const QString& playerName)
{
    if (!m_hasCurrent || !m_db)
        return {};
    const auto json = m_db->getPlayChecks(m_current.id, playerName.toStdString());
    if (!json)
        return {};
    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(*json));
    return doc.array().toVariantList();
}

void AppController::savePlayChecks(const QString& playerName, const QVariantList& checks)
{
    if (!m_hasCurrent || !m_db)
        return;
    core::normalizeGridDims(m_current);
    // Évite d'écraser une grille valide avec un tableau à plat / mal formé (QML).
    if (!play::playChecksLookValid(checks, m_current.gridRows, m_current.gridCols)) {
        qWarning("savePlayChecks: matrice invalide (%d×%d attendu) — ignorée",
                 m_current.gridRows, m_current.gridCols);
        return;
    }
    const QJsonDocument doc(QJsonArray::fromVariantList(checks));
    m_db->savePlayChecks(m_current.id, playerName.toStdString(),
                         doc.toJson(QJsonDocument::Compact).toStdString());
    rememberPlayChecksSnapshot();
    publishPlayChecksIfShared();
}

QVariantMap AppController::togglePlayCell(const QString& playerName, int row, int col)
{
    if (!m_hasCurrent || !m_db)
        return {
            { QStringLiteral("checked"), false },
            { QStringLiteral("checks"), QVariantList{} },
            { QStringLiteral("overlays"), QVariantList{} },
        };
    auto out = play::togglePlayCell(m_current, m_db.get(), playerName, row, col);
    if (!out.result.contains(QStringLiteral("label")))
        return out.result;

    if (!out.newWinners.isEmpty())
        announceWinners(out.newWinners, out.scoreboard);
    rememberPlayChecksSnapshot();
    if (m_projectSync && m_db->getSyncKey(m_current.id)) {
        m_projectSync->setOutboundPlayOverlays(
            QString::fromStdString(m_current.id),
            out.outboundOverlays);
    }
    emit playChecksChanged();
    publishPlayChecksIfShared();
    return out.result;
}

void AppController::announceWinners(const QVariantList& newWinners,
                                    const QVariantList& scoreboard)
{
    if (newWinners.isEmpty())
        return;
    const QString msg = play::winnersToastMessage(newWinners);
    if (msg.isEmpty())
        return;
    emit toast(msg);
    platformNotify(QStringLiteral("Open Bingo"), msg);
    emit playWinnersTriggered(newWinners, scoreboard);
}

void AppController::rememberPlayChecksSnapshot()
{
    m_playChecksSnapshot.clear();
    if (!m_hasCurrent || !m_db)
        return;
    m_playChecksSnapshot = m_db->getAllPlayChecks(m_current.id);
}

void AppController::resetPlayChecks(const QString& /*playerName*/)
{
    if (!m_hasCurrent || !m_db)
        return;
    play::resetAllPlayChecks(m_current, m_db.get());
    rememberPlayChecksSnapshot();
    emit playChecksChanged();
    publishPlayChecksIfShared();
}

int AppController::computeScore(const QString& playerName, const QVariantList& checks)
{
    if (!m_hasCurrent)
        return 0;
    return play::computeScore(m_current, playerName, checks);
}

QVariantList AppController::detectBingoLines(const QVariantList& checks)
{
    if (!m_hasCurrent)
        return {};
    return play::detectBingoLines(m_current, checks);
}

QVariantList AppController::playScoreboard() const
{
    if (!m_hasCurrent || !m_db)
        return {};
    return play::buildScoreboard(m_current, m_db.get());
}


QImage AppController::renderScoreboardImage()
{
    if (!m_hasCurrent || !m_db)
        return {};
    return exportprint::renderScoreboardImage(m_current, m_db.get());
}

bool AppController::exportScoreboardPng(const QString& filePath)
{
    if (!m_hasCurrent || !m_db) {
        emit toast(QStringLiteral("Aucun projet ouvert"));
        return false;
    }
    const QImage img = renderScoreboardImage();
    if (img.isNull()) {
        emit toast(QStringLiteral("Aucun joueur — générez des grilles d'abord"));
        return false;
    }

    QString path = filePath;
    if (!path.endsWith(QLatin1String(".png"), Qt::CaseInsensitive))
        path += QStringLiteral(".png");
    if (!img.save(path, "PNG")) {
        emit toast(QStringLiteral("Échec de l'export PNG"));
        return false;
    }
    return QFileInfo::exists(path) && QFileInfo(path).size() > 200;
}

QString AppController::scoreboardPreviewUrl() const
{
    if (m_scoreboardPreviewPath.isEmpty())
        return {};
    return QUrl::fromLocalFile(m_scoreboardPreviewPath).toString()
         + QStringLiteral("#r=") + QString::number(m_scoreboardPreviewRevision);
}

QString AppController::scoreboardShareLabel() const
{
#ifdef Q_OS_ANDROID
    return QStringLiteral("Partager");
#else
    return QStringLiteral("Enregistrer");
#endif
}

QString AppController::prepareScoreboardPreview()
{
    if (!m_hasCurrent || m_current.grids.empty()) {
        emit toast(QStringLiteral("Générez des grilles d'abord"));
        return {};
    }
    const QImage img = renderScoreboardImage();
    if (img.isNull()) {
        emit toast(QStringLiteral("Aucun joueur — générez des grilles d'abord"));
        return {};
    }

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(dir);
    const QString path = QDir(dir).filePath(QStringLiteral("openbingo-scores-preview.png"));
    if (!img.save(path, "PNG") || !QFileInfo::exists(path) || QFileInfo(path).size() < 200) {
        emit toast(QStringLiteral("Échec de l'aperçu PNG"));
        return {};
    }
    m_scoreboardPreviewPath = path;
    ++m_scoreboardPreviewRevision;
    emit scoreboardPreviewChanged();
    return scoreboardPreviewUrl();
}

bool AppController::shareScoreboardPng()
{
    if (!m_hasCurrent || m_current.grids.empty()) {
        emit toast(QStringLiteral("Générez des grilles d'abord"));
        return false;
    }

#ifdef Q_OS_ANDROID
    if (m_scoreboardPreviewPath.isEmpty() || !QFileInfo::exists(m_scoreboardPreviewPath)
            || QFileInfo(m_scoreboardPreviewPath).size() < 200) {
        if (prepareScoreboardPreview().isEmpty())
            return false;
    }
    if (platformShareImage(m_scoreboardPreviewPath)) {
        emit toast(QStringLiteral("Classement prêt à partager"));
        return true;
    }
    emit toast(QStringLiteral("Impossible de partager le PNG"));
    return false;
#else
    const QString suggested = QDir(
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
        .filePath(QString::fromStdString(m_current.title)
                      .replace(QLatin1Char('/'), QLatin1Char('-'))
                  + QStringLiteral("-scores.png"));

    QString path = QFileDialog::getSaveFileName(
        nullptr,
        QStringLiteral("Exporter le classement en PNG"),
        suggested,
        QStringLiteral("Images PNG (*.png)"));
    if (path.isEmpty())
        return false;
    if (!path.endsWith(QLatin1String(".png"), Qt::CaseInsensitive))
        path += QStringLiteral(".png");

    if (!exportScoreboardPng(path))
        return false;
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    emit toast(QStringLiteral("Classement exporté"));
    return true;
#endif
}

bool AppController::saveScoreboardPng()
{
    return shareScoreboardPng();
}

void AppController::setKeepScreenOn(bool on) { platformKeepScreenOn(on); }

void AppController::lockLandscape() { platformLockLandscape(); }

void AppController::unlockOrientation() { platformUnlockOrientation(); }

void AppController::setImmersive(bool on) { platformSetImmersive(on); }

void AppController::vibrate() { platformVibrate(35); }

void AppController::copyToClipboard(const QString& text)
{
    // Écriture explicite uniquement (bouton Copier). Ne jamais lire le presse-papiers
    // au démarrage / ouverture de projet : Android 12+ affiche alors une notif système
    // du type « a collé depuis le presse-papiers ».
    if (auto* cb = QGuiApplication::clipboard())
        cb->setText(text);
}

QString AppController::detectLineType(const QVariantList& lineCoords, int gridSize) const
{
    if (lineCoords.isEmpty() || gridSize <= 0)
        return {};
    QSet<int> rows, cols;
    for (const QVariant& v : lineCoords) {
        const QVariantList p = v.toList();
        if (p.size() < 2)
            continue;
        rows.insert(p[0].toInt());
        cols.insert(p[1].toInt());
    }
    if (rows.size() == 1)
        return QStringLiteral("line");
    if (cols.size() == 1)
        return QStringLiteral("column");
    bool mainDiag = true, antiDiag = true;
    for (const QVariant& v : lineCoords) {
        const QVariantList p = v.toList();
        if (p.size() < 2)
            continue;
        const int r = p[0].toInt(), c = p[1].toInt();
        if (r != c)
            mainDiag = false;
        if (r + c != gridSize - 1)
            antiDiag = false;
    }
    if (mainDiag || antiDiag)
        return QStringLiteral("diagonal");
    return {};
}

bool AppController::shareText(const QString& text)
{
    if (platformShare(text))
        return true;
    copyToClipboard(text);
    emit toast(QStringLiteral("Lien copié"));
    return true;
}

bool AppController::exportPdf(const QString& filePath)
{
    if (!m_hasCurrent || m_current.grids.empty())
        return false;
    return exportprint::writePdf(m_current, filePath);
}

bool AppController::savePdf()
{
    if (!m_hasCurrent || m_current.grids.empty()) {
        emit toast(QStringLiteral("Générez des grilles d'abord"));
        return false;
    }

    const QString suggested = QDir(
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
        .filePath(QString::fromStdString(m_current.title)
                      .replace(QLatin1Char('/'), QLatin1Char('-'))
                  + QStringLiteral("-bingo.pdf"));

    QString path = QFileDialog::getSaveFileName(
        nullptr,
        QStringLiteral("Enregistrer les grilles en PDF"),
        suggested,
        QStringLiteral("PDF (*.pdf)"));
    if (path.isEmpty())
        return false;
    if (!path.endsWith(QLatin1String(".pdf"), Qt::CaseInsensitive))
        path += QStringLiteral(".pdf");

    if (!exportPdf(path)) {
        emit toast(QStringLiteral("Échec de l'export PDF"));
        return false;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    emit toast(QStringLiteral("PDF enregistré"));
    return true;
}

bool AppController::printGrids()
{
    if (!m_hasCurrent || m_current.grids.empty()) {
        emit toast(QStringLiteral("Générez des grilles d'abord"));
        return false;
    }

#ifdef Q_OS_ANDROID
    const QString path = QDir(
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation))
        .filePath(QStringLiteral("openbingo-print.pdf"));
    if (!exportPdf(path)) {
        emit toast(QStringLiteral("Échec de la préparation PDF"));
        return false;
    }
    if (platformPrintPdf(path)) {
        emit toast(QStringLiteral("Impression…"));
        return true;
    }
    // Repli : ouvrir le PDF (l'utilisateur pourra Imprimer depuis le lecteur).
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    emit toast(QStringLiteral("PDF prêt — utilisez Imprimer dans le lecteur"));
    return true;
#else
    QPrinter printer(QPrinter::HighResolution);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageOrientation(QPageLayout::Portrait);
    printer.setPageMargins(QMarginsF(8, 6, 8, 6), QPageLayout::Millimeter);

    QPrintPreviewDialog dlg(&printer);
    dlg.setWindowTitle(QStringLiteral("Aperçu avant impression — Open Bingo"));
    QObject::connect(&dlg, &QPrintPreviewDialog::paintRequested,
                     this, [this](QPrinter* pr) {
        if (pr && m_hasCurrent)
            exportprint::paintBingoDocument(*pr, m_current);
    });
    dlg.resize(1000, 700);
    dlg.exec();
    return true;
#endif
}

bool AppController::saveScreenshot(const QString& filePath)
{
    const auto windows = QGuiApplication::topLevelWindows();
    if (windows.isEmpty())
        return false;
    auto* window = qobject_cast<QQuickWindow*>(windows.first());
    if (!window)
        return false;
    return window->grabWindow().save(filePath);
}

QString AppController::formatRelativeDate(qint64 ms) const
{
    const QDateTime dt = QDateTime::fromMSecsSinceEpoch(ms);
    const qint64 secs = dt.secsTo(QDateTime::currentDateTime());
    if (secs < 60)
        return QStringLiteral("à l'instant");
    if (secs < 3600)
        return QStringLiteral("il y a %1 min").arg(secs / 60);
    if (secs < 86400)
        return QStringLiteral("il y a %1 h").arg(secs / 3600);
    return dt.toString(QStringLiteral("dd/MM/yyyy"));
}

QString AppController::settingsGet(const QString& key, const QString& defaultValue) const
{
    if (!m_db || key.isEmpty())
        return defaultValue;
    const auto v = m_db->getSetting(key.toStdString());
    if (!v)
        return defaultValue;
    return QString::fromStdString(*v);
}

void AppController::settingsSet(const QString& key, const QString& value)
{
    if (!m_db || key.isEmpty())
        return;
    m_db->setSetting(key.toStdString(), value.toStdString());
}

} // namespace app
