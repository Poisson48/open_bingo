#include "appcontroller.h"

#include "../core/generator.h"
#include "../core/jsoncodec.h"
#include "../core/sharecodec.h"
#include "platform.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QImage>
#include <QJsonArray>
#include <QLinearGradient>
#include <QLocale>
#include <QRadialGradient>
#include <QSet>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPen>
#include <QPrinter>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTextOption>
#include <QUrl>
#include <QtGlobal>
#include <algorithm>
#include <map>
#include <random>
#include <set>
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

std::vector<std::vector<bool>> checksFromVariant(const QVariantList& checks, int rows, int cols)
{
    std::vector<std::vector<bool>> out(rows, std::vector<bool>(cols, false));
    // Grille à plat (bug / vieux JSON) : refuser plutôt que lire n'importe quoi.
    if (!checks.isEmpty() && !checks[0].canConvert<QVariantList>())
        return out;
    for (int r = 0; r < rows && r < checks.size(); ++r) {
        const QVariantList row = checks[r].toList();
        for (int c = 0; c < cols && c < row.size(); ++c)
            out[static_cast<size_t>(r)][static_cast<size_t>(c)] = row[c].toBool();
    }
    return out;
}

bool playChecksLookValid(const QVariantList& checks, int rows, int cols)
{
    if (rows <= 0 || cols <= 0)
        return false;
    if (checks.size() != rows)
        return false;
    for (int r = 0; r < rows; ++r) {
        if (!checks[r].canConvert<QVariantList>())
            return false;
        if (checks[r].toList().size() != cols)
            return false;
    }
    return true;
}

QVariantList checksToVariant(const std::vector<std::vector<bool>>& checks)
{
    QVariantList out;
    for (const auto& row : checks) {
        QVariantList r;
        for (bool v : row)
            r.append(v);
        // QVariantList::append(QVariantList) aplatit — encapsuler comme ailleurs.
        out.append(QVariant(r));
    }
    return out;
}

std::vector<std::vector<bool>> emptyChecks(int rows, int cols, bool freeCenter)
{
    std::vector<std::vector<bool>> out(rows, std::vector<bool>(cols, false));
    if (freeCenter && rows % 2 == 1 && cols % 2 == 1) {
        const int midR = rows / 2;
        const int midC = cols / 2;
        out[static_cast<size_t>(midR)][static_cast<size_t>(midC)] = true;
    }
    return out;
}

QString normalizeCellLabel(const std::string& label)
{
    return QString::fromStdString(label).trimmed();
}

QVariantMap gageOverlayForCell(const core::Project& project, const core::GridCell& cell,
                               const QString& playerName)
{
    QVariantMap m;
    if (cell.isFree)
        return m;
    QString desc;
    int num = cell.points;
    int rateShown = 100;
    if (!cell.gage.empty()) {
        desc = QString::fromStdString(cell.gage);
    } else if (project.gageMode && num > 0) {
        // Tirage pondéré parmi les gages partageant ce n°.
        // Seed stable (projet + joueur + libellé + n°) : même résultat sur tous
        // les appareils quand la coche arrive via sync.
        std::vector<size_t> idxs;
        int total = 0;
        for (size_t i = 0; i < project.gages.size(); ++i) {
            const auto& g = project.gages[i];
            if (g.number != num)
                continue;
            idxs.push_back(i);
            total += qMax(0, g.rate);
        }
        if (idxs.empty())
            return m;
        size_t pick = idxs[0];
        if (idxs.size() > 1) {
            const QByteArray seedBytes = (QString::fromStdString(project.id) + QLatin1Char('|')
                                         + QString::fromStdString(cell.label) + QLatin1Char('|')
                                         + QString::number(num))
                                            .toUtf8();
            const quint64 seed = qHash(seedBytes, /*seed*/ quint64(0x0b1660u));
            std::mt19937 eng{ static_cast<std::mt19937::result_type>(seed) };
            if (total <= 0) {
                std::uniform_int_distribution<size_t> dist(0, idxs.size() - 1);
                pick = idxs[dist(eng)];
            } else {
                std::uniform_int_distribution<int> dist(1, total);
                int roll = dist(eng);
                int acc = 0;
                for (size_t i : idxs) {
                    acc += qMax(0, project.gages[i].rate);
                    if (roll <= acc) {
                        pick = i;
                        break;
                    }
                }
            }
        }
        const auto& g = project.gages[pick];
        desc = QString::fromStdString(g.description);
        rateShown = g.rate;
        // Compter combien de variantes pour ce n°
        m.insert(QStringLiteral("variants"), static_cast<int>(idxs.size()));
    }
    if (desc.isEmpty())
        return m;
    m.insert(QStringLiteral("kind"), QStringLiteral("gage"));
    m.insert(QStringLiteral("num"), num);
    m.insert(QStringLiteral("label"), QString::fromStdString(cell.label));
    m.insert(QStringLiteral("desc"), desc);
    m.insert(QStringLiteral("player"), playerName);
    m.insert(QStringLiteral("rate"), rateShown);
    return m;
}

QString formatPlayerList(const QStringList& names)
{
    if (names.isEmpty())
        return {};
    if (names.size() == 1)
        return names[0];
    if (names.size() == 2)
        return names[0] + QStringLiteral(" et ") + names[1];
    QStringList head = names.mid(0, names.size() - 1);
    return head.join(QStringLiteral(", ")) + QStringLiteral(" et ") + names.last();
}

// Regroupe gages / combos : une notif par événement (libellé coché),
// pas une par variante de tirage — sinon « moi » puis « les autres ».
QVariantList groupPlayOverlays(const QVariantList& raw)
{
    QList<QVariantMap> groups;
    QHash<QString, int> indexByKey;

    auto appendPlayer = [&](QVariantMap ov, const QString& groupKey) {
        const QString player = ov.value(QStringLiteral("player")).toString();
        const QString desc = ov.value(QStringLiteral("desc")).toString().trimmed();
        if (player.isEmpty() || desc.isEmpty())
            return;
        if (!indexByKey.contains(groupKey)) {
            QStringList players;
            players << player;
            ov.insert(QStringLiteral("players"), players);
            ov.remove(QStringLiteral("player"));
            indexByKey.insert(groupKey, groups.size());
            groups.append(ov);
        } else {
            QVariantMap& g = groups[indexByKey.value(groupKey)];
            QStringList players = g.value(QStringLiteral("players")).toStringList();
            if (!players.contains(player)) {
                players << player;
                g.insert(QStringLiteral("players"), players);
            }
        }
    };

    for (const QVariant& v : raw) {
        const QVariantMap ov = v.toMap();
        const QString kind = ov.value(QStringLiteral("kind")).toString();
        const QString desc = ov.value(QStringLiteral("desc")).toString().trimmed();
        if (kind == QLatin1String("gage")) {
            // Même case cochée (libellé) → une seule carte pour tout le monde.
            const QString label = ov.value(QStringLiteral("label")).toString().trimmed();
            appendPlayer(ov, QStringLiteral("gage\n") + label.toLower());
        } else if (kind == QLatin1String("combo")) {
            // Même type de combo + même texte → une carte.
            appendPlayer(ov, QStringLiteral("combo\n")
                                 + ov.value(QStringLiteral("key")).toString()
                                 + QLatin1Char('\n') + desc);
        }
    }

    QVariantList out;
    for (QVariantMap g : groups) {
        const QStringList players = g.value(QStringLiteral("players")).toStringList();
        const QString who = formatPlayerList(players);
        const QString verb = players.size() <= 1 ? QStringLiteral("doit")
                                                 : QStringLiteral("doivent");
        g.insert(QStringLiteral("who"), who);
        g.insert(QStringLiteral("verb"), verb);
        g.insert(QStringLiteral("prompt"),
                 who + QLatin1Char(' ') + verb + QStringLiteral(" :"));
        if (!players.isEmpty())
            g.insert(QStringLiteral("player"), players.join(QStringLiteral(", ")));
        out.append(g);
    }
    return out;
}

std::set<QString> bingoTypeSet(const core::Project& project,
                               const std::vector<std::vector<bool>>& checks)
{
    std::set<QString> types;
    const int rows = project.gridRows;
    const int cols = project.gridCols;
    const auto lines = core::detectBingo(checks, rows, cols);
    for (const auto& line : lines) {
        if (line.empty())
            continue;
        bool sameRow = true;
        bool sameCol = true;
        bool mainDiag = true;
        bool antiDiag = true;
        const int r0 = line[0].first;
        const int c0 = line[0].second;
        for (const auto& [r, c] : line) {
            if (r != r0) sameRow = false;
            if (c != c0) sameCol = false;
            if (r != c) mainDiag = false;
            if (rows == cols) {
                if (r + c != rows - 1) antiDiag = false;
            } else {
                antiDiag = false;
                mainDiag = false;
            }
        }
        if (sameRow)
            types.insert(QStringLiteral("line"));
        else if (sameCol)
            types.insert(QStringLiteral("column"));
        else if ((mainDiag || antiDiag) && rows == cols)
            types.insert(QStringLiteral("diagonal"));
    }
    return types;
}

bool isGridFull(const std::vector<std::vector<bool>>& checks, int N)
{
    if (N <= 0 || static_cast<int>(checks.size()) < N)
        return false;
    for (int r = 0; r < N; ++r) {
        if (static_cast<int>(checks[static_cast<size_t>(r)].size()) < N)
            return false;
        for (int c = 0; c < N; ++c) {
            if (!checks[static_cast<size_t>(r)][static_cast<size_t>(c)])
                return false;
        }
    }
    return true;
}

int countChecked(const std::vector<std::vector<bool>>& checks)
{
    int n = 0;
    for (const auto& row : checks)
        for (bool v : row)
            if (v) ++n;
    return n;
}

int countPlayableCells(const core::PlayerGrid& grid)
{
    int n = 0;
    for (const auto& row : grid.cells)
        for (const auto& cell : row)
            if (!cell.isFree)
                ++n;
    return n;
}

int countCheckedPlayable(const core::PlayerGrid& grid,
                         const std::vector<std::vector<bool>>& checks)
{
    int n = 0;
    const int rows = static_cast<int>(grid.cells.size());
    for (int r = 0; r < rows; ++r) {
        const auto& crow = grid.cells[static_cast<size_t>(r)];
        for (int c = 0; c < static_cast<int>(crow.size()); ++c) {
            if (crow[static_cast<size_t>(c)].isFree)
                continue;
            if (r < static_cast<int>(checks.size())
                && c < static_cast<int>(checks[static_cast<size_t>(r)].size())
                && checks[static_cast<size_t>(r)][static_cast<size_t>(c)])
                ++n;
        }
    }
    return n;
}

QVariantList buildScoreboard(const core::Project& project, store::Database* db)
{
    QVariantList board;
    if (!db)
        return board;
    const int rows = project.gridRows;
    const int cols = project.gridCols;
    // Mode gage : les « points » des cases sont des n° de gage — on classe
    // sur les cases cochées, pas sur la somme des n°.
    const bool gageMode = project.gageMode;
    for (const auto& grid : project.grids) {
        auto checks = emptyChecks(rows, cols, core::projectHasFreeCenter(project));
        if (const auto json = db->getPlayChecks(project.id, grid.player)) {
            const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(*json));
            checks = checksFromVariant(doc.array().toVariantList(), rows, cols);
            if (core::projectHasFreeCenter(project)) {
                const int midR = rows / 2;
                const int midC = cols / 2;
                checks[static_cast<size_t>(midR)][static_cast<size_t>(midC)] = true;
            }
        }
        const int total = countPlayableCells(grid);
        const int checked = countCheckedPlayable(grid, checks);
        const bool full = total > 0 && checked >= total;
        const int score = gageMode ? checked : core::computeScore(grid, checks);
        board.append(QVariantMap{
            { QStringLiteral("player"), QString::fromStdString(grid.player) },
            { QStringLiteral("score"), score },
            { QStringLiteral("checked"), checked },
            { QStringLiteral("total"), total },
            { QStringLiteral("full"), full },
            { QStringLiteral("gageMode"), gageMode },
            { QStringLiteral("unit"), gageMode ? QStringLiteral("cases")
                                              : QStringLiteral("pts") },
        });
    }
    std::sort(board.begin(), board.end(), [](const QVariant& a, const QVariant& b) {
        const auto ma = a.toMap();
        const auto mb = b.toMap();
        if (ma.value(QStringLiteral("full")).toBool() != mb.value(QStringLiteral("full")).toBool())
            return ma.value(QStringLiteral("full")).toBool();
        const int sa = ma.value(QStringLiteral("score")).toInt();
        const int sb = mb.value(QStringLiteral("score")).toInt();
        if (sa != sb)
            return sa > sb;
        return ma.value(QStringLiteral("checked")).toInt()
            > mb.value(QStringLiteral("checked")).toInt();
    });
    return board;
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

    m_projectModel = std::make_unique<ProjectModel>(this);
    m_relayPool = std::make_unique<net::RelayPool>(this);
    m_projectSync = std::make_unique<ProjectSync>(this);
    m_updater = std::make_unique<Updater>(this);

    std::string deviceIdStr = m_db->getSetting("device_id").value_or("");
    if (deviceIdStr.empty()) {
        deviceIdStr = core::JsonCodec::makeId();
        m_db->setSetting("device_id", deviceIdStr);
    }
    const QString deviceId = QString::fromStdString(deviceIdStr);

    if (!screenshotMode) {
        m_projectSync->init(m_db.get(), m_relayPool.get(), deviceId);
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
                                overlaysForNewlyCheckedCells(before, after);
                        }
                        m_playChecksSnapshot = after;
                        emit currentProjectChanged();
                        emit gridsChanged();
                        emit playChecksChanged();
                        if (!remoteOverlays.isEmpty())
                            emit playOverlaysTriggered(remoteOverlays);
                    }
                });
        m_updater->check();
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
        m_lastTab = 3; // Grilles
        emit lastTabChanged();
        emit toast(QStringLiteral("Projet créé avec grilles — regénérez après vos modifications."));
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
    clone.grids.clear();
    clone.createdAt = QDateTime::currentMSecsSinceEpoch();
    clone.updatedAt = clone.createdAt;
    m_db->upsertProject(clone);
    reloadProjects();
    return QString::fromStdString(clone.id);
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
        const auto empty = checksToVariant(emptyChecks(m_current.gridRows, m_current.gridCols,
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
        auto checks = emptyChecks(rows, cols, core::projectHasFreeCenter(m_current));
        if (const auto json = m_db->getPlayChecks(m_current.id, player)) {
            const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(*json));
            checks = checksFromVariant(doc.array().toVariantList(), rows, cols);
        }
        if (r1 < rows && r2 < rows && c1 < cols && c2 < cols) {
            const bool tmp = checks[static_cast<size_t>(r1)][static_cast<size_t>(c1)];
            checks[static_cast<size_t>(r1)][static_cast<size_t>(c1)] =
                checks[static_cast<size_t>(r2)][static_cast<size_t>(c2)];
            checks[static_cast<size_t>(r2)][static_cast<size_t>(c2)] = tmp;
            m_db->savePlayChecks(m_current.id, player,
                                 QJsonDocument(QJsonArray::fromVariantList(checksToVariant(checks)))
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

QString ensureJsonSuffix(QString path)
{
    if (!path.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive))
        path += QStringLiteral(".json");
    return path;
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

void AppController::enableProjectSharing()
{
    if (m_hasCurrent && m_projectSync)
        m_projectSync->enableSharing(currentProjectId());
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
    if (!playChecksLookValid(checks, m_current.gridRows, m_current.gridCols)) {
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
    // Pattern Colo Courses/Tâches (ItemModel::toggleDone) : une seule API de cochage,
    // persistance immédiate, signal pour rafraîchir l'UI. Ici l'identité partagée
    // n'est pas un itemId mais le libellé de case (même film → même événement).
    QVariantMap result;
    result.insert(QStringLiteral("checked"), false);
    result.insert(QStringLiteral("checks"), QVariantList{});
    result.insert(QStringLiteral("overlays"), QVariantList{});

    if (!m_hasCurrent || !m_db || playerName.isEmpty())
        return result;
    core::normalizeGridDims(m_current);
    const int rows = m_current.gridRows;
    const int cols = m_current.gridCols;
    if (rows <= 0 || cols <= 0 || row < 0 || col < 0 || row >= rows || col >= cols)
        return result;

    const core::PlayerGrid* sourceGrid = nullptr;
    for (const auto& g : m_current.grids) {
        if (g.player == playerName.toStdString()) {
            sourceGrid = &g;
            break;
        }
    }
    if (!sourceGrid || row >= static_cast<int>(sourceGrid->cells.size())
        || col >= static_cast<int>(sourceGrid->cells[static_cast<size_t>(row)].size()))
        return result;

    const auto& sourceCell = sourceGrid->cells[static_cast<size_t>(row)][static_cast<size_t>(col)];
    if (sourceCell.isFree)
        return result;

    const QString label = normalizeCellLabel(sourceCell.label);
    if (label.isEmpty())
        return result;

    auto loadOrEmpty = [&](const std::string& pname) {
        const auto json = m_db->getPlayChecks(m_current.id, pname);
        if (!json)
            return emptyChecks(rows, cols, core::projectHasFreeCenter(m_current));
        const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(*json));
        const QVariantList raw = doc.array().toVariantList();
        // Matrice corrompue / à plat : ne pas repartir de zéro (effacerait les coches).
        if (!playChecksLookValid(raw, rows, cols)) {
            qWarning("play checks invalides pour %s — conservation d'une grille vide non écrasante",
                     pname.c_str());
            // Tenter une lecture partielle plutôt qu'un wipe total.
        }
        auto checks = checksFromVariant(raw, rows, cols);
        if (core::projectHasFreeCenter(m_current)) {
            const int midR = rows / 2;
            const int midC = cols / 2;
            checks[static_cast<size_t>(midR)][static_cast<size_t>(midC)] = true;
        }
        return checks;
    };

    auto viewerChecksBefore = loadOrEmpty(playerName.toStdString());
    const bool newChecked = !viewerChecksBefore[static_cast<size_t>(row)][static_cast<size_t>(col)];

    // Combos avant cochage — pour chaque joueur (ligne / colonne / diagonale).
    std::map<std::string, std::set<QString>> comboTypesBefore;
    for (const auto& grid : m_current.grids)
        comboTypesBefore[grid.player] = bingoTypeSet(m_current, loadOrEmpty(grid.player));

    // Qui avait déjà une grille pleine (pour détecter les nouveaux gagnants).
    QSet<QString> fullBefore;
    for (const QVariant& rowV : buildScoreboard(m_current, m_db.get())) {
        const auto m = rowV.toMap();
        if (m.value(QStringLiteral("full")).toBool())
            fullBefore.insert(m.value(QStringLiteral("player")).toString());
    }

    QVariantList overlays;
    QVariantList viewerChecksOut;
    // Un overlay gage par joueur touché par le libellé (pas seulement celui
    // qui a tapé) — les autres doivent aussi faire leur gage.
    QSet<QString> gagePlayersSeen;

    for (const auto& grid : m_current.grids) {
        auto checks = loadOrEmpty(grid.player);
        const QString pname = QString::fromStdString(grid.player);

        for (int r = 0; r < rows && r < static_cast<int>(grid.cells.size()); ++r) {
            const auto& crow = grid.cells[static_cast<size_t>(r)];
            for (int c = 0; c < cols && c < static_cast<int>(crow.size()); ++c) {
                const auto& cell = crow[static_cast<size_t>(c)];
                if (cell.isFree)
                    continue;
                if (normalizeCellLabel(cell.label) != label)
                    continue;
                const bool was = checks[static_cast<size_t>(r)][static_cast<size_t>(c)];
                checks[static_cast<size_t>(r)][static_cast<size_t>(c)] = newChecked;
                if (newChecked && !was && m_current.gageMode
                    && !gagePlayersSeen.contains(pname)) {
                    QVariantMap ov = gageOverlayForCell(m_current, cell, pname);
                    if (!ov.isEmpty()) {
                        gagePlayersSeen.insert(pname);
                        const bool isTap = (grid.player == playerName.toStdString()
                                            && r == row && c == col);
                        if (isTap)
                            overlays.insert(0, ov);
                        else
                            overlays.append(ov);
                    }
                }
            }
        }

        if (core::projectHasFreeCenter(m_current)) {
            const int midR = rows / 2;
            const int midC = cols / 2;
            checks[static_cast<size_t>(midR)][static_cast<size_t>(midC)] = true;
        }

        const QVariantList asVar = checksToVariant(checks);
        m_db->savePlayChecks(m_current.id, grid.player,
                             QJsonDocument(QJsonArray::fromVariantList(asVar))
                                 .toJson(QJsonDocument::Compact)
                                 .toStdString());
        if (grid.player == playerName.toStdString())
            viewerChecksOut = asVar;
    }

    // Combos nouvellement débloqués chez n'importe quel joueur.
    if (newChecked && m_current.gageMode) {
        const auto& combos = m_current.comboGages;
        auto pushCombo = [&](const QString& pname, const char* key, const std::string& text,
                             const std::set<QString>& oldTypes, const std::set<QString>& newTypes) {
            if (text.empty())
                return;
            const QString k = QString::fromUtf8(key);
            if (!newTypes.count(k) || oldTypes.count(k))
                return;
            QVariantMap ov;
            ov.insert(QStringLiteral("kind"), QStringLiteral("combo"));
            ov.insert(QStringLiteral("key"), k);
            ov.insert(QStringLiteral("label"),
                      k == QLatin1String("line") ? QStringLiteral("Ligne complète")
                      : k == QLatin1String("column") ? QStringLiteral("Colonne complète")
                                                     : QStringLiteral("Diagonale complète"));
            ov.insert(QStringLiteral("desc"), QString::fromStdString(text));
            ov.insert(QStringLiteral("player"), pname);
            overlays.append(ov);
        };
        for (const auto& grid : m_current.grids) {
            const auto after = loadOrEmpty(grid.player);
            const auto newTypes = bingoTypeSet(m_current, after);
            const auto& oldTypes = comboTypesBefore[grid.player];
            const QString pname = QString::fromStdString(grid.player);
            pushCombo(pname, "line", combos.line, oldTypes, newTypes);
            pushCombo(pname, "column", combos.column, oldTypes, newTypes);
            pushCombo(pname, "diagonal", combos.diagonal, oldTypes, newTypes);
        }
    }

    const auto board = buildScoreboard(m_current, m_db.get());
    bool viewerFull = false;
    QVariantList winners;
    QVariantList newWinners;
    for (const QVariant& rowV : board) {
        const auto m = rowV.toMap();
        if (!m.value(QStringLiteral("full")).toBool())
            continue;
        const QString pname = m.value(QStringLiteral("player")).toString();
        winners.append(pname);
        if (pname == playerName)
            viewerFull = true;
        if (newChecked && !fullBefore.contains(pname))
            newWinners.append(m); // map complète (player, score, …)
    }

    if (!newWinners.isEmpty()) {
        QStringList names;
        for (const QVariant& w : newWinners)
            names << w.toMap().value(QStringLiteral("player")).toString();
        const QString msg = names.size() == 1
            ? (names[0] + QStringLiteral(" a gagné — grille complète !"))
            : (names.join(QStringLiteral(", ")) + QStringLiteral(" ont gagné !"));
        emit toast(msg);
    }

    result.insert(QStringLiteral("checked"), newChecked);
    result.insert(QStringLiteral("checks"), viewerChecksOut);
    const QVariantList groupedOverlays = groupPlayOverlays(overlays);
    result.insert(QStringLiteral("overlays"), groupedOverlays);
    result.insert(QStringLiteral("label"), label);
    result.insert(QStringLiteral("gridFull"), viewerFull);
    result.insert(QStringLiteral("justCompleted"), !newWinners.isEmpty());
    result.insert(QStringLiteral("newWinners"), newWinners);
    result.insert(QStringLiteral("winners"), winners);
    result.insert(QStringLiteral("scoreboard"), board);
    rememberPlayChecksSnapshot();
    // Publier les overlays avec les coches : les autres téléphones affichent
    // exactement la même liste de noms (pas de recalcul local divergent).
    if (m_projectSync && m_db && m_db->getSyncKey(m_current.id)) {
        m_projectSync->setOutboundPlayOverlays(
            QString::fromStdString(m_current.id),
            newChecked ? groupedOverlays : QVariantList{});
    }
    emit playChecksChanged();
    publishPlayChecksIfShared();
    return result;
}

void AppController::rememberPlayChecksSnapshot()
{
    m_playChecksSnapshot.clear();
    if (!m_hasCurrent || !m_db)
        return;
    m_playChecksSnapshot = m_db->getAllPlayChecks(m_current.id);
}

QVariantList AppController::overlaysForNewlyCheckedCells(
    const std::map<std::string, std::string>& before,
    const std::map<std::string, std::string>& after) const
{
    QVariantList raw;
    if (!m_hasCurrent || !m_current.gageMode)
        return raw;
    const int rows = m_current.gridRows;
    const int cols = m_current.gridCols;
    if (rows <= 0 || cols <= 0)
        return raw;

    auto parse = [&](const std::string& player,
                     const std::map<std::string, std::string>& map) {
        const auto it = map.find(player);
        if (it == map.end())
            return emptyChecks(rows, cols, core::projectHasFreeCenter(m_current));
        const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(it->second));
        auto checks = checksFromVariant(doc.array().toVariantList(), rows, cols);
        if (static_cast<int>(checks.size()) != rows)
            return emptyChecks(rows, cols, core::projectHasFreeCenter(m_current));
        if (core::projectHasFreeCenter(m_current)) {
            const int midR = rows / 2;
            const int midC = cols / 2;
            checks[static_cast<size_t>(midR)][static_cast<size_t>(midC)] = true;
        }
        return checks;
    };

    QSet<QString> gagePlayersSeen;
    const auto& combos = m_current.comboGages;
    auto pushCombo = [&](const QString& pname, const char* key, const std::string& text,
                         const std::set<QString>& oldTypes, const std::set<QString>& newTypes) {
        if (text.empty())
            return;
        const QString k = QString::fromUtf8(key);
        if (!newTypes.count(k) || oldTypes.count(k))
            return;
        QVariantMap ov;
        ov.insert(QStringLiteral("kind"), QStringLiteral("combo"));
        ov.insert(QStringLiteral("key"), k);
        ov.insert(QStringLiteral("label"),
                  k == QLatin1String("line") ? QStringLiteral("Ligne complète")
                  : k == QLatin1String("column") ? QStringLiteral("Colonne complète")
                                                 : QStringLiteral("Diagonale complète"));
        ov.insert(QStringLiteral("desc"), QString::fromStdString(text));
        ov.insert(QStringLiteral("player"), pname);
        raw.append(ov);
    };

    for (const auto& grid : m_current.grids) {
        const QString pname = QString::fromStdString(grid.player);
        const auto wasChecks = parse(grid.player, before);
        const auto nowChecks = parse(grid.player, after);
        const auto oldTypes = bingoTypeSet(m_current, wasChecks);
        const auto newTypes = bingoTypeSet(m_current, nowChecks);

        for (int r = 0; r < rows && r < static_cast<int>(grid.cells.size()); ++r) {
            const auto& crow = grid.cells[static_cast<size_t>(r)];
            for (int c = 0; c < cols && c < static_cast<int>(crow.size()); ++c) {
                const auto& cell = crow[static_cast<size_t>(c)];
                if (cell.isFree)
                    continue;
                const bool was = wasChecks[static_cast<size_t>(r)][static_cast<size_t>(c)];
                const bool now = nowChecks[static_cast<size_t>(r)][static_cast<size_t>(c)];
                if (!now || was)
                    continue;
                if (gagePlayersSeen.contains(pname))
                    continue;
                QVariantMap ov = gageOverlayForCell(m_current, cell, pname);
                if (ov.isEmpty())
                    continue;
                gagePlayersSeen.insert(pname);
                raw.append(ov);
            }
        }

        pushCombo(pname, "line", combos.line, oldTypes, newTypes);
        pushCombo(pname, "column", combos.column, oldTypes, newTypes);
        pushCombo(pname, "diagonal", combos.diagonal, oldTypes, newTypes);
    }
    return groupPlayOverlays(raw);
}

void AppController::resetPlayChecks(const QString& /*playerName*/)
{
    if (!m_hasCurrent || !m_db)
        return;
    const int rows = m_current.gridRows;
    const int cols = m_current.gridCols;
    // Toujours toutes les grilles : un libellé coché l'est chez tous les joueurs,
    // donc une remise à zéro partielle laisserait la partie incohérente.
    for (const auto& g : m_current.grids) {
        const auto empty = checksToVariant(emptyChecks(rows, cols, core::projectHasFreeCenter(m_current)));
        m_db->savePlayChecks(m_current.id, g.player,
                             QJsonDocument(QJsonArray::fromVariantList(empty))
                                 .toJson(QJsonDocument::Compact)
                                 .toStdString());
    }
    rememberPlayChecksSnapshot();
    emit playChecksChanged();
    publishPlayChecksIfShared();
}

int AppController::computeScore(const QString& playerName, const QVariantList& checks)
{
    if (!m_hasCurrent)
        return 0;
    const int rows = m_current.gridRows;
    const int cols = m_current.gridCols;
    for (const auto& grid : m_current.grids) {
        if (grid.player != playerName.toStdString())
            continue;
        return core::computeScore(grid, checksFromVariant(checks, rows, cols));
    }
    return 0;
}

QVariantList AppController::detectBingoLines(const QVariantList& checks)
{
    if (!m_hasCurrent)
        return {};
    const int rows = m_current.gridRows;
    const int cols = m_current.gridCols;
    const auto lines = core::detectBingo(checksFromVariant(checks, rows, cols), rows, cols);
    QVariantList out;
    for (const auto& line : lines) {
        QVariantList coords;
        for (const auto& [r, c] : line)
            coords.append(QVariant(QVariantList{ r, c }));
        out.append(QVariant(coords));
    }
    return out;
}

QVariantList AppController::playScoreboard() const
{
    if (!m_hasCurrent || !m_db)
        return {};
    return buildScoreboard(m_current, m_db.get());
}

QImage AppController::renderScoreboardImage()
{
    if (!m_hasCurrent || !m_db)
        return {};
    const QVariantList board = buildScoreboard(m_current, m_db.get());
    if (board.isEmpty())
        return {};

    const bool gageMode = m_current.gageMode;
    const QString unit = gageMode ? QStringLiteral("cases") : QStringLiteral("pts");
    QStringList winnerNames;
    for (const QVariant& rowV : board) {
        const auto m = rowV.toMap();
        if (m.value(QStringLiteral("full")).toBool())
            winnerNames << m.value(QStringLiteral("player")).toString();
    }

    const int W = 1000;
    const int pad = 40;

    auto fontPx = [](int px, bool bold = false) {
        QFont f = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
        f.setPixelSize(px);
        f.setBold(bold);
        return f;
    };
    auto elide = [](const QFont& f, const QString& t, int maxW) {
        return QFontMetrics(f).elidedText(t, Qt::ElideRight, maxW);
    };
    auto fmtScore = [&](int score) {
        return QLocale(QLocale::French).toString(score) + QLatin1Char(' ') + unit;
    };

    const QString title = QString::fromStdString(m_current.title).trimmed();
    const QString description = QString::fromStdString(m_current.description).trimmed();

    QFont titleFont = fontPx(32, true);
    QFont subFont = fontPx(14);
    QFont descFont = fontPx(15);

    const int titleTop = 24;
    const int titleH = 42;
    int cursorY = titleTop + titleH;

    QRect descRect;
    if (!description.isEmpty()) {
        cursorY += 4;
        const int descMaxW = W - 2 * pad;
        const QFontMetrics fm(descFont);
        const QRect bounds = fm.boundingRect(QRect(0, 0, descMaxW, 80),
                                             Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                                             description);
        const int descBlockH = qBound(20, bounds.height() + 4, 72);
        descRect = QRect(pad, cursorY, descMaxW, descBlockH);
        cursorY += descBlockH + 6;
    } else {
        cursorY += 4;
    }

    const int metaY = cursorY;
    const int metaH = 24;
    cursorY += metaH + 10;

    const int winnerBannerH = winnerNames.isEmpty() ? 0 : 72;
    const int headerH = cursorY + (winnerNames.isEmpty() ? 0 : winnerBannerH);
    const int podiumH = board.size() >= 1 ? 220 : 0;
    const int rowH = 70;
    const int footerH = 48;
    const int listStart = headerH + podiumH;
    const int H = listStart + board.size() * rowH + footerH;

    QImage img(W, H, QImage::Format_RGB32);
    img.fill(QColor(QStringLiteral("#0f1623")));

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // Accent top
    QLinearGradient bar(0, 0, W, 0);
    bar.setColorAt(0.0, QColor(QStringLiteral("#4f46e5")));
    bar.setColorAt(1.0, QColor(QStringLiteral("#22c55e")));
    p.fillRect(0, 0, W, 8, bar);

    p.setFont(titleFont);
    p.setPen(QColor(QStringLiteral("#f1f5f9")));
    p.drawText(QRect(pad, titleTop, W - 2 * pad, titleH), Qt::AlignLeft | Qt::AlignVCenter,
               elide(titleFont, title.isEmpty() ? QStringLiteral("Bingo") : title, W - 2 * pad));

    if (!description.isEmpty()) {
        p.setFont(descFont);
        p.setPen(QColor(QStringLiteral("#cbd5e1")));
        p.drawText(descRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, description);
    }

    p.setFont(subFont);
    p.setPen(QColor(QStringLiteral("#7c8fa6")));
    p.drawText(QRect(pad, metaY, W / 2, metaH), Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("Classement · Open Bingo"));
    p.drawText(QRect(W / 2, metaY, W / 2 - pad, metaH), Qt::AlignRight | Qt::AlignVCenter,
               QDateTime::currentDateTime().toString(QStringLiteral("dd/MM/yyyy  HH:mm")));

    int y = metaY + metaH + 10;
    if (!winnerNames.isEmpty()) {
        const QRectF banner(pad, y, W - 2 * pad, 56);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(34, 197, 94, 45));
        p.drawRoundedRect(banner, 14, 14);
        p.setPen(QPen(QColor(QStringLiteral("#22c55e")), 2));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(banner.adjusted(1, 1, -1, -1), 14, 14);

        QFont winFont = fontPx(18, true);
        p.setFont(winFont);
        p.setPen(QColor(QStringLiteral("#22c55e")));
        const QString winText = winnerNames.size() == 1
            ? (QStringLiteral("Gagnant : ") + winnerNames[0])
            : (QStringLiteral("Gagnants : ") + winnerNames.join(QStringLiteral(" · ")));
        p.drawText(banner.toRect().adjusted(18, 0, -18, 0), Qt::AlignVCenter | Qt::AlignLeft,
                   elide(winFont, winText, static_cast<int>(banner.width()) - 36));
        y += winnerBannerH;
    }

    // Podium top 3
    if (podiumH > 0 && board.size() >= 1) {
        const int podiumTop = y + 8;
        const int baseY = podiumTop + podiumH - 24;
        const int mid = W / 2;

        auto drawPodiumBlock = [&](int rankIdx, int cx, int blockH, const QColor& color) {
            if (rankIdx < 0 || rankIdx >= board.size())
                return;
            const auto m = board[rankIdx].toMap();
            const QString name = m.value(QStringLiteral("player")).toString();
            const int score = m.value(QStringLiteral("score")).toInt();
            const bool full = m.value(QStringLiteral("full")).toBool();
            const int bw = 170;
            const QRect block(cx - bw / 2, baseY - blockH, bw, blockH);
            p.setPen(Qt::NoPen);
            p.setBrush(color);
            p.drawRoundedRect(block, 12, 12);

            QFont rankF = fontPx(28, true);
            p.setFont(rankF);
            p.setPen(QColor(QStringLiteral("#0f1623")));
            p.drawText(QRect(block.x(), block.y() + 10, block.width(), 36), Qt::AlignCenter,
                       QString::number(rankIdx + 1));

            QFont nameF = fontPx(13, true);
            p.setFont(nameF);
            p.setPen(QColor(QStringLiteral("#0f1623")));
            p.drawText(QRect(block.x() + 8, block.y() + 48, block.width() - 16, 36),
                       Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap,
                       elide(nameF, name, block.width() - 16));

            QFont scoreF = fontPx(15, true);
            p.setFont(scoreF);
            p.setPen(full ? QColor(QStringLiteral("#14532d")) : QColor(QStringLiteral("#0f1623")));
            p.drawText(QRect(block.x() + 6, block.bottom() - 34, block.width() - 12, 28),
                       Qt::AlignCenter, elide(scoreF, fmtScore(score), block.width() - 12));
        };

        // Ordre visuel : 2 | 1 | 3
        if (board.size() >= 2)
            drawPodiumBlock(1, mid - 220, 110, QColor(QStringLiteral("#94a3b8")));
        drawPodiumBlock(0, mid, 150, QColor(QStringLiteral("#fbbf24")));
        if (board.size() >= 3)
            drawPodiumBlock(2, mid + 220, 90, QColor(QStringLiteral("#d97706")));
        y = listStart;
    } else {
        y = listStart;
    }

    // Liste complète
    for (int i = 0; i < board.size(); ++i) {
        const auto m = board[i].toMap();
        const QString name = m.value(QStringLiteral("player")).toString();
        const int score = m.value(QStringLiteral("score")).toInt();
        const int checked = m.value(QStringLiteral("checked")).toInt();
        const int total = qMax(1, m.value(QStringLiteral("total")).toInt());
        const bool full = m.value(QStringLiteral("full")).toBool();
        const int rank = i + 1;

        const QRectF rowRect(pad, y, W - 2 * pad, rowH - 10);
        p.setPen(Qt::NoPen);
        p.setBrush(full ? QColor(QStringLiteral("#1a2e24"))
                        : (i % 2 == 0 ? QColor(QStringLiteral("#1a2235"))
                                      : QColor(QStringLiteral("#151c2c"))));
        p.drawRoundedRect(rowRect, 12, 12);
        if (full) {
            p.setPen(QPen(QColor(QStringLiteral("#22c55e")), 2));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(rowRect.adjusted(1, 1, -1, -1), 12, 12);
        }

        QColor medal = QColor(QStringLiteral("#3d5270"));
        if (rank == 1) medal = QColor(QStringLiteral("#fbbf24"));
        else if (rank == 2) medal = QColor(QStringLiteral("#e2e8f0"));
        else if (rank == 3) medal = QColor(QStringLiteral("#d97706"));
        const QRectF medalRect(pad + 16, y + 14, 36, 36);
        p.setPen(Qt::NoPen);
        p.setBrush(medal);
        p.drawEllipse(medalRect);
        QFont rankFont = fontPx(14, true);
        p.setFont(rankFont);
        p.setPen(rank <= 3 ? QColor(QStringLiteral("#0f1623")) : QColor(QStringLiteral("#f1f5f9")));
        p.drawText(medalRect.toRect(), Qt::AlignCenter, QString::number(rank));

        const int nameX = pad + 66;
        const int scoreColW = 200;
        const int nameW = W - 2 * pad - 66 - scoreColW - 16;

        QFont nameFont = fontPx(full ? 17 : 16, full || rank <= 3);
        p.setFont(nameFont);
        p.setPen(QColor(QStringLiteral("#f1f5f9")));
        const QString nameDraw = full ? (name + QStringLiteral("  ·  gagnant")) : name;
        p.drawText(QRect(nameX, y + 6, nameW, 26), Qt::AlignLeft | Qt::AlignVCenter,
                   elide(nameFont, nameDraw, nameW));

        // Barre de progression (cases cochées / total jouables)
        const int barW = nameW;
        const int barX = nameX;
        const int barY = y + 40;
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(QStringLiteral("#0b1220")));
        p.drawRoundedRect(QRect(barX, barY, barW, 12), 6, 6);
        const int fillW = qBound(0, static_cast<int>(barW * (checked / static_cast<double>(total))), barW);
        p.setBrush(full ? QColor(QStringLiteral("#22c55e")) : QColor(QStringLiteral("#6366f1")));
        if (fillW > 0)
            p.drawRoundedRect(QRect(barX, barY, fillW, 12), 6, 6);

        QFont progFont = fontPx(11);
        p.setFont(progFont);
        p.setPen(QColor(QStringLiteral("#7c8fa6")));
        p.drawText(QRect(nameX, y + 54, nameW, 14), Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("%1 / %2").arg(checked).arg(total));

        QFont scoreFont = fontPx(20, true);
        p.setFont(scoreFont);
        p.setPen(full ? QColor(QStringLiteral("#22c55e")) : QColor(QStringLiteral("#a5b4fc")));
        p.drawText(QRect(W - pad - scoreColW - 8, y, scoreColW, rowH - 10),
                   Qt::AlignRight | Qt::AlignVCenter, fmtScore(score));

        y += rowH;
    }

    p.setPen(QColor(QStringLiteral("#5b6b82")));
    p.setFont(fontPx(12));
    p.drawText(QRect(pad, H - footerH, W - 2 * pad, footerH - 14), Qt::AlignCenter,
               QStringLiteral("Open Bingo  ·  %1 joueur%2%3")
                   .arg(board.size())
                   .arg(board.size() > 1 ? QStringLiteral("s") : QString())
                   .arg(winnerNames.isEmpty()
                            ? QString()
                            : QStringLiteral("  ·  %1 gagnant%2")
                                  .arg(winnerNames.size())
                                  .arg(winnerNames.size() > 1 ? QStringLiteral("s") : QString())));
    p.end();
    return img;
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

namespace {

qreal mmToPx(const QPrinter& printer, qreal mm)
{
    return mm * printer.resolution() / 25.4;
}

void drawPlayerSheet(QPainter& p, const QRectF& area, const core::Project& project,
                     const core::PlayerGrid& grid)
{
    const bool gageMode = project.gageMode;
    const bool hasGages = !project.gages.empty();
    qreal y = area.top();
    const qreal left = area.left();
    const qreal w = area.width();

    // En-tête : titre projet + nom joueur
    QFont titleFont(QStringLiteral("Sans Serif"));
    titleFont.setBold(true);
    titleFont.setPixelSize(qMax(10, int(area.height() * 0.032)));
    p.setFont(titleFont);
    p.setPen(Qt::black);
    p.drawText(QRectF(left, y, w, titleFont.pixelSize() * 1.35),
               Qt::AlignHCenter | Qt::AlignVCenter,
               QString::fromStdString(project.title));
    y += titleFont.pixelSize() * 1.4;

    QFont playerFont(titleFont);
    playerFont.setPixelSize(qMax(14, int(area.height() * 0.052)));
    playerFont.setBold(true);
    p.setFont(playerFont);
    p.drawText(QRectF(left, y, w, playerFont.pixelSize() * 1.25),
               Qt::AlignHCenter | Qt::AlignVCenter,
               QString::fromStdString(grid.player));
    y += playerFont.pixelSize() * 1.35;

    p.setPen(QPen(Qt::black, 2.2));
    p.drawLine(QPointF(left, y), QPointF(left + w, y));
    y += 8;

    // Pied réservé (HP / règles / note gage)
    const qreal footerH = area.height() * (gageMode ? 0.11 : 0.26);
    const qreal gridBottom = area.bottom() - footerH;
    QRectF gridArea(left, y, w, qMax(20.0, gridBottom - y));

    if (grid.cells.empty() || gridArea.height() < 20)
        return;

    const int rowsN = static_cast<int>(grid.cells.size());
    const int colsN = rowsN > 0
        ? static_cast<int>(grid.cells[0].size()) : 0;
    if (rowsN <= 0 || colsN <= 0)
        return;
    // Cases carrées centrées dans la zone réservée
    const qreal side = qMin(gridArea.width() / colsN, gridArea.height() / rowsN);
    const qreal gridW = side * colsN;
    const qreal gridH = side * rowsN;
    gridArea = QRectF(left + (w - gridW) / 2,
                      y + qMax(0.0, (gridArea.height() - gridH) / 2),
                      gridW, gridH);

    const int N = qMax(rowsN, colsN);
    const qreal fontPx = qMax(6.0, side * (N <= 3 ? 0.22 : N <= 5 ? 0.18 : 0.15));
    const qreal ptsPx = qMax(5.0, fontPx * 0.72);

    QFont cellFont(QStringLiteral("Sans Serif"));
    cellFont.setPixelSize(int(fontPx));
    QFont ptsFont(cellFont);
    ptsFont.setPixelSize(int(ptsPx));
    ptsFont.setBold(true);

    QTextOption opt;
    opt.setWrapMode(QTextOption::WordWrap);
    opt.setAlignment(Qt::AlignCenter);

    for (int r = 0; r < rowsN; ++r) {
        const auto& row = grid.cells[static_cast<size_t>(r)];
        for (int c = 0; c < colsN && c < static_cast<int>(row.size()); ++c) {
            const auto& cell = row[static_cast<size_t>(c)];
            const QRectF rect(gridArea.left() + c * side,
                              gridArea.top() + r * side,
                              side, side);
            if (cell.isFree)
                p.fillRect(rect, QColor(QStringLiteral("#e8e8e8")));
            else
                p.fillRect(rect, Qt::white);
            p.setPen(QPen(Qt::black, 2.0));
            p.drawRect(rect);

            const QString label = cell.isFree
                ? QStringLiteral("Libre")
                : QString::fromStdString(cell.label);
            p.setFont(cellFont);
            p.setPen(Qt::black);
            p.drawText(rect.adjusted(3, 2, -3, -ptsPx - 3), label, opt);

            if (!cell.isFree) {
                const QString pts = gageMode
                    ? (QStringLiteral("#") + QString::number(cell.points))
                    : QString::number(cell.points);
                p.setFont(ptsFont);
                p.setPen(gageMode ? QColor(QStringLiteral("#4f46e5"))
                                  : QColor(QStringLiteral("#555555")));
                p.drawText(rect.adjusted(2, 2, -4, -3),
                           Qt::AlignBottom | Qt::AlignRight, pts);
            }
        }
    }

    // Pied
    y = qMax(gridArea.bottom(), gridBottom - footerH) + 6;
    if (y > area.bottom() - 8)
        y = area.bottom() - footerH + 4;
    p.setPen(QPen(QColor(QStringLiteral("#bbbbbb")), 1));
    p.drawLine(QPointF(left, y), QPointF(left + w, y));
    y += 6;

    QFont small(QStringLiteral("Sans Serif"));
    small.setPixelSize(qMax(8, int(area.height() * 0.02)));
    QFont smallBold(small);
    smallBold.setBold(true);

    if (gageMode) {
        p.setFont(small);
        p.setPen(QColor(QStringLiteral("#333333")));
        QTextOption noteOpt;
        noteOpt.setWrapMode(QTextOption::WordWrap);
        p.drawText(QRectF(left, y, w, area.bottom() - y),
                   QStringLiteral("Le n° dans le coin bas-droit de chaque case est le "
                                  "numéro du gage à effectuer (voir la feuille « Tableau des Gages »)."),
                   noteOpt);
        return;
    }

    // Mode classique : cases PV + multiplicateurs (tableau 2×2 comme l'app web)
    p.setFont(smallBold);
    p.setPen(Qt::black);
    p.drawText(QRectF(left, y, w, smallBold.pixelSize() * 1.25),
               Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("Points de vie — %1 PV").arg(project.startHP));
    y += smallBold.pixelSize() * 1.4;

    const qreal box = qMax(9.0, small.pixelSize() * 1.15);
    const int hp = qBound(0, project.startHP, 80);
    qreal x = left;
    p.setPen(QPen(Qt::black, 1.4));
    for (int i = 0; i < hp; ++i) {
        if (x + box > left + w) {
            x = left;
            y += box + 3;
            if (y + box > area.bottom() - small.pixelSize() * 6)
                break;
        }
        p.drawRect(QRectF(x, y, box, box));
        x += box + 3;
    }
    y += box + 7;

    p.setFont(smallBold);
    p.drawText(QRectF(left, y, w, smallBold.pixelSize() * 1.25),
               Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("Règles des combinaisons"));
    y += smallBold.pixelSize() * 1.35;

    p.setFont(small);
    const qreal colW = w * 0.5;
    const qreal rowH = small.pixelSize() * 1.45;
    auto ruleRow = [&](const QString& a, const QString& av, const QString& b, const QString& bv) {
        if (y + rowH > area.bottom())
            return;
        p.drawText(QRectF(left, y, colW * 0.55, rowH), Qt::AlignVCenter | Qt::AlignLeft, a);
        p.setFont(smallBold);
        p.drawText(QRectF(left + colW * 0.55, y, colW * 0.45, rowH),
                   Qt::AlignVCenter | Qt::AlignLeft, av);
        p.setFont(small);
        p.drawText(QRectF(left + colW, y, colW * 0.55, rowH), Qt::AlignVCenter | Qt::AlignLeft, b);
        p.setFont(smallBold);
        p.drawText(QRectF(left + colW + colW * 0.55, y, colW * 0.45, rowH),
                   Qt::AlignVCenter | Qt::AlignLeft, bv);
        p.setFont(small);
        y += rowH;
    };
    ruleRow(QStringLiteral("Ligne complète"),
            QStringLiteral("× %1").arg(project.multipliers.line),
            QStringLiteral("Colonne complète"),
            QStringLiteral("× %1").arg(project.multipliers.column));
    ruleRow(QStringLiteral("Diagonale complète"),
            QStringLiteral("× %1").arg(project.multipliers.diagonal),
            QStringLiteral("Grille complète (BINGO)"),
            QStringLiteral("× %1").arg(project.multipliers.full));

    y += 3;
    p.setFont(small);
    p.setPen(QColor(QStringLiteral("#444444")));
    QTextOption rulesOpt;
    rulesOpt.setWrapMode(QTextOption::WordWrap);
    const QString note = QStringLiteral(
        "Coche une case quand l'événement se produit. La valeur en points est "
        "dans le coin bas-droit.%1")
        .arg(hasGages
                 ? QStringLiteral(" Pour récupérer des PV, accomplis un gage "
                                  "(feuille « Tableau des Gages »).")
                 : QString());
    p.drawText(QRectF(left, y, w, area.bottom() - y), note, rulesOpt);
}

void drawGageSheet(QPainter& p, const QRectF& area, const core::Project& project)
{
    qreal y = area.top();
    const qreal left = area.left();
    const qreal w = area.width();

    QFont titleFont(QStringLiteral("Sans Serif"));
    titleFont.setBold(true);
    titleFont.setPixelSize(qMax(10, int(area.height() * 0.03)));
    p.setFont(titleFont);
    p.setPen(Qt::black);
    p.drawText(QRectF(left, y, w, titleFont.pixelSize() * 1.4),
               Qt::AlignHCenter | Qt::AlignVCenter,
               QString::fromStdString(project.title));
    y += titleFont.pixelSize() * 1.5;

    QFont playerFont(titleFont);
    playerFont.setPixelSize(qMax(14, int(area.height() * 0.045)));
    p.setFont(playerFont);
    p.drawText(QRectF(left, y, w, playerFont.pixelSize() * 1.3),
               Qt::AlignHCenter | Qt::AlignVCenter,
               QStringLiteral("Tableau des Gages"));
    y += playerFont.pixelSize() * 1.5;

    p.setPen(QPen(Qt::black, 2));
    p.drawLine(QPointF(left, y), QPointF(left + w, y));
    y += 10;

    QFont body(QStringLiteral("Sans Serif"));
    body.setPixelSize(qMax(9, int(area.height() * 0.025)));
    QFont bodyItalic(body);
    bodyItalic.setItalic(true);
    p.setFont(bodyItalic);
    p.setPen(QColor(QStringLiteral("#333333")));
    const QString intro = project.gageMode
        ? QStringLiteral("Le n° sur chaque case tire un gage parmi ceux qui portent "
                         "ce numéro (selon le %). Effectue-le quand tu tombes dessus !")
        : QStringLiteral("Accomplis n'importe quel gage pour récupérer des points "
                         "de vie. Une fois accompli, coche-le.");
    QTextOption introOpt;
    introOpt.setWrapMode(QTextOption::WordWrap);
    const qreal introH = body.pixelSize() * 3.2;
    p.drawText(QRectF(left, y, w, introH), intro, introOpt);
    y += introH + 8;

    const bool showHp = !project.gageMode;
    const bool showRate = project.gageMode;
    const qreal colNum = w * 0.10;
    const qreal colRate = showRate ? w * 0.12 : 0;
    const qreal colHp = showHp ? w * 0.16 : 0;
    const qreal colDesc = w - colNum - colRate - colHp;
    const qreal rowH = qMax(18.0, body.pixelSize() * 2.0);

    auto drawHeaderCell = [&](const QRectF& r, const QString& text) {
        p.fillRect(r, QColor(QStringLiteral("#f0f0f0")));
        p.setPen(QPen(Qt::black, 1.2));
        p.drawRect(r);
        QFont h = body;
        h.setBold(true);
        p.setFont(h);
        p.drawText(r.adjusted(4, 0, -4, 0), Qt::AlignVCenter | Qt::AlignLeft, text);
    };

    drawHeaderCell(QRectF(left, y, colNum, rowH), QStringLiteral("#"));
    drawHeaderCell(QRectF(left + colNum, y, colDesc, rowH), QStringLiteral("Gage"));
    if (showRate)
        drawHeaderCell(QRectF(left + colNum + colDesc, y, colRate, rowH),
                       QStringLiteral("%"));
    if (showHp)
        drawHeaderCell(QRectF(left + colNum + colDesc + colRate, y, colHp, rowH),
                       QStringLiteral("PV"));
    y += rowH;

    p.setFont(body);
    for (size_t i = 0; i < project.gages.size(); ++i) {
        if (y + rowH > area.bottom())
            break;
        const auto& g = project.gages[i];
        const QRectF rNum(left, y, colNum, rowH);
        const QRectF rDesc(left + colNum, y, colDesc, rowH);
        const QRectF rRate(left + colNum + colDesc, y, colRate, rowH);
        const QRectF rHp(left + colNum + colDesc + colRate, y, colHp, rowH);
        p.setPen(QPen(Qt::black, 1));
        p.drawRect(rNum);
        p.drawRect(rDesc);
        if (showRate)
            p.drawRect(rRate);
        if (showHp)
            p.drawRect(rHp);
        p.drawText(rNum, Qt::AlignCenter, QString::number(g.number));
        p.drawText(rDesc.adjusted(6, 2, -4, -2), Qt::AlignVCenter | Qt::AlignLeft,
                   QString::fromStdString(g.description));
        if (showRate)
            p.drawText(rRate, Qt::AlignCenter, QString::number(g.rate) + QLatin1Char('%'));
        if (showHp)
            p.drawText(rHp, Qt::AlignCenter,
                       QStringLiteral("+%1 PV").arg(g.hp));
        y += rowH;
    }

    const auto& combos = project.comboGages;
    if (project.gageMode
        && (!combos.line.empty() || !combos.column.empty() || !combos.diagonal.empty())) {
        y += 14;
        QFont comboTitle(body);
        comboTitle.setBold(true);
        p.setFont(comboTitle);
        p.setPen(Qt::black);
        p.drawText(QRectF(left, y, w, comboTitle.pixelSize() * 1.4),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("Gages de combinaison"));
        y += comboTitle.pixelSize() * 1.6;
        p.setFont(body);
        auto addCombo = [&](const char* label, const std::string& text) {
            if (text.empty() || y + rowH > area.bottom())
                return;
            const QRectF rType(left, y, w * 0.28, rowH);
            const QRectF rText(left + w * 0.28, y, w * 0.72, rowH);
            p.setPen(QPen(Qt::black, 1));
            p.drawRect(rType);
            p.drawRect(rText);
            QFont bold = body;
            bold.setBold(true);
            p.setFont(bold);
            p.drawText(rType.adjusted(4, 0, -4, 0), Qt::AlignVCenter | Qt::AlignLeft,
                       QString::fromUtf8(label));
            p.setFont(body);
            p.drawText(rText.adjusted(6, 0, -4, 0), Qt::AlignVCenter | Qt::AlignLeft,
                       QString::fromStdString(text));
            y += rowH;
        };
        addCombo("Ligne complète", combos.line);
        addCombo("Colonne complète", combos.column);
        addCombo("Diagonale complète", combos.diagonal);
    }
}

void paintBingoDocument(QPrinter& printer, const core::Project& project)
{
    QPainter painter;
    if (!painter.begin(&printer))
        return;

    const QRectF page = printer.pageRect(QPrinter::DevicePixel);
    const int perPage = 2;
    // Bande centrale pour découper les deux demi-feuilles A5 (≈148,5 mm).
    const qreal cutBand = mmToPx(printer, 5);
    const qreal halfH = (page.height() - cutBand) / perPage;
    const qreal insetX = mmToPx(printer, 4); // padding horizontal type web (15 mm − marge page)
    const qreal insetY = mmToPx(printer, 2);

    auto drawCutGuide = [&](qreal midY) {
        const qreal y = midY;
        QPen dash(QColor(QStringLiteral("#888888")), 1.2, Qt::DashLine);
        dash.setDashPattern({ 4, 3 });
        painter.setPen(dash);
        painter.drawLine(QPointF(page.left(), y), QPointF(page.right(), y));

        // Petites marques « ciseaux » aux extrémités
        QFont mark(QStringLiteral("Sans Serif"));
        mark.setPixelSize(qMax(8, int(mmToPx(printer, 2.8))));
        painter.setFont(mark);
        painter.setPen(QColor(QStringLiteral("#666666")));
        const QString scissors = QStringLiteral("✂ découper");
        painter.drawText(QRectF(page.left(), y - mark.pixelSize() * 0.7,
                                page.width() * 0.45, mark.pixelSize() * 1.4),
                         Qt::AlignLeft | Qt::AlignVCenter, scissors);
        painter.drawText(QRectF(page.left() + page.width() * 0.55, y - mark.pixelSize() * 0.7,
                                page.width() * 0.45, mark.pixelSize() * 1.4),
                         Qt::AlignRight | Qt::AlignVCenter, scissors);
    };

    int slot = 0;
    for (size_t i = 0; i < project.grids.size(); ++i) {
        if (slot == perPage) {
            if (!printer.newPage()) {
                painter.end();
                return;
            }
            slot = 0;
        }
        if (slot == 1)
            drawCutGuide(page.top() + halfH + cutBand * 0.5);

        const qreal top = page.top() + slot * (halfH + cutBand);
        const QRectF area(page.left() + insetX,
                          top + insetY,
                          page.width() - 2 * insetX,
                          halfH - 2 * insetY);
        drawPlayerSheet(painter, area, project, project.grids[i]);
        ++slot;
    }

    if (!project.gages.empty()) {
        if (!printer.newPage()) {
            painter.end();
            return;
        }
        const QRectF gageArea(page.left() + insetX,
                              page.top() + insetY,
                              page.width() - 2 * insetX,
                              page.height() - 2 * insetY);
        drawGageSheet(painter, gageArea, project);
    }

    painter.end();
}

} // namespace

bool AppController::exportPdf(const QString& filePath)
{
    if (!m_hasCurrent || m_current.grids.empty())
        return false;
    if (filePath.isEmpty())
        return false;

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(filePath);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageOrientation(QPageLayout::Portrait);
    printer.setPageMargins(QMarginsF(8, 6, 8, 6), QPageLayout::Millimeter);

    paintBingoDocument(printer, m_current);
    return QFile::exists(filePath) && QFileInfo(filePath).size() > 500;
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
            paintBingoDocument(*pr, m_current);
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

} // namespace app
