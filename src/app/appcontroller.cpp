#include "appcontroller.h"

#include "../core/generator.h"
#include "../core/jsoncodec.h"
#include "../core/sharecodec.h"
#include "platform.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <random>
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
#include <set>
#include <random>
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

std::vector<std::vector<bool>> checksFromVariant(const QVariantList& checks, int N)
{
    std::vector<std::vector<bool>> out(N, std::vector<bool>(N, false));
    for (int r = 0; r < N && r < checks.size(); ++r) {
        const QVariantList row = checks[r].toList();
        for (int c = 0; c < N && c < row.size(); ++c)
            out[r][c] = row[c].toBool();
    }
    return out;
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

std::vector<std::vector<bool>> emptyChecks(int N, bool freeCenter)
{
    std::vector<std::vector<bool>> out(N, std::vector<bool>(N, false));
    if (freeCenter && N % 2 == 1) {
        const int mid = N / 2;
        out[static_cast<size_t>(mid)][static_cast<size_t>(mid)] = true;
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
            thread_local std::mt19937 eng{ std::random_device{}() };
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

std::set<QString> bingoTypeSet(const core::Project& project,
                               const std::vector<std::vector<bool>>& checks)
{
    std::set<QString> types;
    const int N = project.gridSize;
    const auto lines = core::detectBingo(checks, N);
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
            if (r + c != N - 1) antiDiag = false;
        }
        if (sameRow)
            types.insert(QStringLiteral("line"));
        else if (sameCol)
            types.insert(QStringLiteral("column"));
        else if (mainDiag || antiDiag)
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
    const int N = project.gridSize;
    // Mode gage : les « points » des cases sont des n° de gage — on classe
    // sur les cases cochées, pas sur la somme des n°.
    const bool gageMode = project.gageMode;
    for (const auto& grid : project.grids) {
        auto checks = emptyChecks(N, project.freeCenter);
        if (const auto json = db->getPlayChecks(project.id, grid.player)) {
            const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(*json));
            checks = checksFromVariant(doc.array().toVariantList(), N);
            if (project.freeCenter && N % 2 == 1) {
                const int mid = N / 2;
                checks[static_cast<size_t>(mid)][static_cast<size_t>(mid)] = true;
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
                    if (m_hasCurrent && m_current.id == id.toStdString()) {
                        // Ne pas forcer l'onglet Play : l'utilisateur peut être en Config.
                        openProject(id, false);
                        // grids NOTIFY gridsChanged seulement — sans ça l'UI garde l'ancien aperçu.
                        ++m_gridsRevision;
                        emit gridsChanged();
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
            emit toast(QStringLiteral("Projet démo chargé — cochez des cases dans Play !"));
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

void AppController::setGridSize(int v)
{
    if (!m_hasCurrent)
        return;
    m_current.gridSize = qBound(2, v, 12);
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
    p.description = "Bingo des clichés qui arrivent dans le film — 4 joueurs, grille 5×5.";
    p.gridSize = 5;
    p.startHP = 20;
    p.freeCenter = true;
    p.gageMode = false;
    p.players = { { "Léa" }, { "Max" }, { "Sam" }, { "Chloé" } };
    // Gages prêts si on active le mode gage (points des cases = n° de gage).
    // Plusieurs gages peuvent partager un n° avec des % de tirage.
    p.gages = {
        { "Imite la voix du personnage", 5, 1, 60 },
        { "Chante la réplique en mode opéra", 5, 1, 40 },
        { "Mime la scène en 10 secondes", 5, 2, 100 },
        { "Inventez la réplique suivante", 5, 3, 70 },
        { "Fais un bruitage live", 5, 3, 30 },
        { "Change de place avec ton voisin", 3, 4, 100 },
        { "Raconte la scène en chuchotant", 3, 5, 100 },
    };
    p.comboGages.line = "Toute la table boit une gorgée";
    p.comboGages.column = "Le joueur à ta gauche invente un titre alternatif";
    p.comboGages.diagonal = "Tout le monde se lève 5 secondes";

    // Clichés / événements DANS le film (pas pendant la séance). Phrases courtes pour mobile.
    static const char* phrases[] = {
        "Le héros se réveille en sursaut",
        "Flashback en noir et blanc",
        "Le méchant monologue",
        "Course-poursuite en voiture",
        "Explosion spectaculaire",
        "Révélation : un traître",
        "Baiser sous la pluie",
        "Le mentor meurt",
        "Montage entraînement",
        "Le chien survit",
        "Twist prévisible",
        "Bagarre au ralenti",
        "Il refuse d'abord la quête",
        "Vilain qui tombe",
        "Réplique culte répétée",
        "Se déguise mal",
        "Fin ouverte",
        "Caméo surprise",
        "Plan produit trop long",
        "Ils se séparent puis se retrouvent",
        "Vision / fantôme",
        "Compte à rebours",
        "Sauvetage in extremis",
        "Gag après le générique",
        "Le vrai méchant était un allié",
    };
    p.cases.clear();
    // N° max parmi les gages (pas l'index) pour assigner les phrases.
    int maxNum = 1;
    for (const auto& g : p.gages)
        maxNum = std::max(maxNum, g.number);
    int i = 0;
    for (const auto* label : phrases) {
        const int gageNum = (i % maxNum) + 1;
        p.cases.push_back({ label, gageNum, 100 });
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
        const int N = p.gridSize;
        const int mid = N / 2;
        QJsonArray rows;
        for (int r = 0; r < N; ++r) {
            QJsonArray row;
            for (int c = 0; c < N; ++c) {
                const bool free = p.freeCenter && (N % 2 == 1) && r == mid && c == mid;
                row.append(free || (r == c));
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
    std::swap(cells[r1][c1], cells[r2][c2]);
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
    if (!m_hasCurrent)
        return false;
    return writeFile(filePath, core::JsonCodec::projectToJson(m_current));
}

bool AppController::importJsonFile(const QString& filePath)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    bool ok = false;
    auto p = core::JsonCodec::projectFromJson(f.readAll().toStdString(), &ok);
    if (!ok)
        return false;
    m_db->upsertProject(p);
    reloadProjects();
    openProject(QString::fromStdString(p.id));
    return true;
}

bool AppController::exportProjectJson(const QString& id, const QString& filePath)
{
    const auto p = m_db->getProject(id.toStdString());
    return p && writeFile(filePath, core::JsonCodec::projectToJson(*p));
}

bool AppController::exportAllJson(const QString& filePath)
{
    return writeFile(filePath, core::JsonCodec::exportAll(m_db->getAllProjects()));
}

int AppController::importAllJsonFile(const QString& filePath)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly))
        return -1;
    bool ok = false;
    const auto projects = core::JsonCodec::importAll(f.readAll().toStdString(), &ok);
    if (!ok)
        return -1;
    for (const auto& p : projects)
        m_db->upsertProject(p);
    reloadProjects();
    return static_cast<int>(projects.size());
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
    const QJsonDocument doc(QJsonArray::fromVariantList(checks));
    m_db->savePlayChecks(m_current.id, playerName.toStdString(),
                         doc.toJson(QJsonDocument::Compact).toStdString());
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
    const int N = m_current.gridSize;
    if (N <= 0 || row < 0 || col < 0 || row >= N || col >= N)
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

    // Snapshot combos avant (joueur courant uniquement).
    auto loadOrEmpty = [&](const std::string& pname) {
        const auto json = m_db->getPlayChecks(m_current.id, pname);
        if (!json)
            return emptyChecks(N, m_current.freeCenter);
        const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(*json));
        auto checks = checksFromVariant(doc.array().toVariantList(), N);
        if (static_cast<int>(checks.size()) != N)
            return emptyChecks(N, m_current.freeCenter);
        if (m_current.freeCenter && N % 2 == 1) {
            const int mid = N / 2;
            checks[static_cast<size_t>(mid)][static_cast<size_t>(mid)] = true;
        }
        return checks;
    };

    auto viewerChecksBefore = loadOrEmpty(playerName.toStdString());
    const bool newChecked = !viewerChecksBefore[static_cast<size_t>(row)][static_cast<size_t>(col)];
    const auto oldTypes = bingoTypeSet(m_current, viewerChecksBefore);

    // Qui avait déjà une grille pleine (pour détecter les nouveaux gagnants).
    QSet<QString> fullBefore;
    for (const QVariant& rowV : buildScoreboard(m_current, m_db.get())) {
        const auto m = rowV.toMap();
        if (m.value(QStringLiteral("full")).toBool())
            fullBefore.insert(m.value(QStringLiteral("player")).toString());
    }

    QVariantList overlays;
    QVariantList viewerChecksOut;

    for (const auto& grid : m_current.grids) {
        auto checks = loadOrEmpty(grid.player);
        const QString pname = QString::fromStdString(grid.player);

        for (int r = 0; r < N && r < static_cast<int>(grid.cells.size()); ++r) {
            const auto& crow = grid.cells[static_cast<size_t>(r)];
            for (int c = 0; c < N && c < static_cast<int>(crow.size()); ++c) {
                const auto& cell = crow[static_cast<size_t>(c)];
                if (cell.isFree)
                    continue;
                if (normalizeCellLabel(cell.label) != label)
                    continue;
                const bool was = checks[static_cast<size_t>(r)][static_cast<size_t>(c)];
                checks[static_cast<size_t>(r)][static_cast<size_t>(c)] = newChecked;
                // Overlay : uniquement le(s) gage(s) du joueur en train de jouer,
                // pas ceux des autres grilles (sync par libellé reste silencieuse).
                if (newChecked && !was && m_current.gageMode
                    && grid.player == playerName.toStdString()) {
                    QVariantMap ov = gageOverlayForCell(m_current, cell, pname);
                    if (!ov.isEmpty()) {
                        const bool isTap = (r == row && c == col);
                        if (isTap)
                            overlays.insert(0, ov);
                        else
                            overlays.append(ov);
                    }
                }
            }
        }

        if (m_current.freeCenter && N % 2 == 1) {
            const int mid = N / 2;
            checks[static_cast<size_t>(mid)][static_cast<size_t>(mid)] = true;
        }

        const QVariantList asVar = checksToVariant(checks);
        m_db->savePlayChecks(m_current.id, grid.player,
                             QJsonDocument(QJsonArray::fromVariantList(asVar))
                                 .toJson(QJsonDocument::Compact)
                                 .toStdString());
        if (grid.player == playerName.toStdString())
            viewerChecksOut = asVar;
    }

    // Combos nouvellement débloqués → overlays séparés (après les gages de cases).
    if (newChecked && m_current.gageMode && !viewerChecksOut.isEmpty()) {
        const auto after = checksFromVariant(viewerChecksOut, N);
        const auto newTypes = bingoTypeSet(m_current, after);
        const auto& combos = m_current.comboGages;
        auto pushCombo = [&](const char* key, const std::string& text) {
            if (text.empty())
                return;
            const QString k = QString::fromUtf8(key);
            if (newTypes.count(k) && !oldTypes.count(k)) {
                QVariantMap ov;
                ov.insert(QStringLiteral("kind"), QStringLiteral("combo"));
                ov.insert(QStringLiteral("key"), k);
                ov.insert(QStringLiteral("label"),
                          k == QLatin1String("line") ? QStringLiteral("Ligne complète")
                          : k == QLatin1String("column") ? QStringLiteral("Colonne complète")
                                                         : QStringLiteral("Diagonale complète"));
                ov.insert(QStringLiteral("desc"), QString::fromStdString(text));
                ov.insert(QStringLiteral("player"), playerName);
                overlays.append(ov);
            }
        };
        pushCombo("line", combos.line);
        pushCombo("column", combos.column);
        pushCombo("diagonal", combos.diagonal);
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
    result.insert(QStringLiteral("overlays"), overlays);
    result.insert(QStringLiteral("label"), label);
    result.insert(QStringLiteral("gridFull"), viewerFull);
    result.insert(QStringLiteral("justCompleted"), !newWinners.isEmpty());
    result.insert(QStringLiteral("newWinners"), newWinners);
    result.insert(QStringLiteral("winners"), winners);
    result.insert(QStringLiteral("scoreboard"), board);
    emit playChecksChanged();
    return result;
}

void AppController::resetPlayChecks(const QString& playerName)
{
    if (!m_hasCurrent || !m_db)
        return;
    const int N = m_current.gridSize;
    // Comme uncheckAll Colo : on remet la grille du joueur (et seulement lui) à zéro.
    // Les libellés partagés se resynchroniseront au prochain cochage.
    if (!playerName.isEmpty()) {
        const auto empty = checksToVariant(emptyChecks(N, m_current.freeCenter));
        m_db->savePlayChecks(m_current.id, playerName.toStdString(),
                             QJsonDocument(QJsonArray::fromVariantList(empty))
                                 .toJson(QJsonDocument::Compact)
                                 .toStdString());
    } else {
        for (const auto& g : m_current.grids) {
            const auto empty = checksToVariant(emptyChecks(N, m_current.freeCenter));
            m_db->savePlayChecks(m_current.id, g.player,
                                 QJsonDocument(QJsonArray::fromVariantList(empty))
                                     .toJson(QJsonDocument::Compact)
                                     .toStdString());
        }
    }
    emit playChecksChanged();
}

int AppController::computeScore(const QString& playerName, const QVariantList& checks)
{
    if (!m_hasCurrent)
        return 0;
    const int N = m_current.gridSize;
    for (const auto& grid : m_current.grids) {
        if (grid.player != playerName.toStdString())
            continue;
        return core::computeScore(grid, checksFromVariant(checks, N));
    }
    return 0;
}

QVariantList AppController::detectBingoLines(const QVariantList& checks)
{
    if (!m_hasCurrent)
        return {};
    const int N = m_current.gridSize;
    const auto lines = core::detectBingo(checksFromVariant(checks, N), N);
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

bool AppController::exportScoreboardPng(const QString& filePath)
{
    if (!m_hasCurrent || !m_db) {
        emit toast(QStringLiteral("Aucun projet ouvert"));
        return false;
    }
    const QVariantList board = buildScoreboard(m_current, m_db.get());
    if (board.isEmpty()) {
        emit toast(QStringLiteral("Aucun joueur — générez des grilles d'abord"));
        return false;
    }

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
    const int podiumH = board.size() >= 1 ? 220 : 0;
    const int winnerBannerH = winnerNames.isEmpty() ? 0 : 72;
    const int headerH = 110 + winnerBannerH;
    const int rowH = 70;
    const int footerH = 48;
    const int listStart = headerH + podiumH;
    const int H = listStart + board.size() * rowH + footerH;

    QImage img(W, H, QImage::Format_RGB32);
    img.fill(QColor(QStringLiteral("#0f1623")));

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

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

    // Accent top
    QLinearGradient bar(0, 0, W, 0);
    bar.setColorAt(0.0, QColor(QStringLiteral("#4f46e5")));
    bar.setColorAt(1.0, QColor(QStringLiteral("#22c55e")));
    p.fillRect(0, 0, W, 8, bar);

    const QString title = QString::fromStdString(m_current.title).trimmed();
    QFont titleFont = fontPx(32, true);
    p.setFont(titleFont);
    p.setPen(QColor(QStringLiteral("#f1f5f9")));
    p.drawText(QRect(pad, 24, W - 2 * pad, 42), Qt::AlignLeft | Qt::AlignVCenter,
               elide(titleFont, title.isEmpty() ? QStringLiteral("Bingo") : title, W - 2 * pad));

    QFont subFont = fontPx(14);
    p.setFont(subFont);
    p.setPen(QColor(QStringLiteral("#7c8fa6")));
    p.drawText(QRect(pad, 70, W / 2, 24), Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("Classement · Open Bingo"));
    p.drawText(QRect(W / 2, 70, W / 2 - pad, 24), Qt::AlignRight | Qt::AlignVCenter,
               QDateTime::currentDateTime().toString(QStringLiteral("dd/MM/yyyy  HH:mm")));

    int y = 104;
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

    QString path = filePath;
    if (!path.endsWith(QLatin1String(".png"), Qt::CaseInsensitive))
        path += QStringLiteral(".png");
    if (!img.save(path, "PNG")) {
        emit toast(QStringLiteral("Échec de l'export PNG"));
        return false;
    }
    return QFileInfo::exists(path) && QFileInfo(path).size() > 200;
}

bool AppController::saveScoreboardPng()
{
    if (!m_hasCurrent || m_current.grids.empty()) {
        emit toast(QStringLiteral("Générez des grilles d'abord"));
        return false;
    }

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

    const int N = static_cast<int>(grid.cells.size());
    // Cases carrées centrées (comme une vraie grille bingo imprimée)
    const qreal side = qMin(gridArea.width() / N, gridArea.height() / N);
    const qreal gridW = side * N;
    const qreal gridH = side * N;
    gridArea = QRectF(left + (w - gridW) / 2,
                      y + qMax(0.0, (gridArea.height() - gridH) / 2),
                      gridW, gridH);

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

    for (int r = 0; r < N; ++r) {
        const auto& row = grid.cells[static_cast<size_t>(r)];
        for (int c = 0; c < N && c < static_cast<int>(row.size()); ++c) {
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
                ? QStringLiteral("FREE")
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
        "Cochez une case quand l'événement se produit. La valeur en points est "
        "dans le coin bas-droit.%1")
        .arg(hasGages
                 ? QStringLiteral(" Pour récupérer des PV, accomplissez un gage "
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
                         "ce numéro (selon le %). Effectuez-le quand vous tombez dessus !")
        : QStringLiteral("Accomplissez n'importe quel gage pour récupérer des points "
                         "de vie. Une fois accompli, cochez-le.");
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
