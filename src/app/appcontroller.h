#pragma once

#include "../core/bingotypes.h"
#include "../store/database.h"
#include "../net/relaypool.h"
#include "projectmodel.h"
#include "projectsync.h"
#include "updater.h"

#include <QObject>
#include <QSet>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <memory>

namespace app {

class AppController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(ProjectModel* projects READ projects CONSTANT)
    Q_PROPERTY(QString currentProjectId READ currentProjectId NOTIFY currentProjectChanged)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY currentProjectChanged)
    Q_PROPERTY(QString description READ description WRITE setDescription NOTIFY currentProjectChanged)
    Q_PROPERTY(int gridSize READ gridSize WRITE setGridSize NOTIFY currentProjectChanged)
    Q_PROPERTY(int startHP READ startHP WRITE setStartHP NOTIFY currentProjectChanged)
    Q_PROPERTY(bool freeCenter READ freeCenter WRITE setFreeCenter NOTIFY currentProjectChanged)
    Q_PROPERTY(bool gageMode READ gageMode WRITE setGageMode NOTIFY currentProjectChanged)
    Q_PROPERTY(bool gridsDirty READ gridsDirty NOTIFY gridsDirtyChanged)
    Q_PROPERTY(int lastTab READ lastTab WRITE setLastTab NOTIFY lastTabChanged)
    Q_PROPERTY(QVariantList players READ players NOTIFY currentProjectChanged)
    Q_PROPERTY(QVariantList cases READ cases NOTIFY currentProjectChanged)
    Q_PROPERTY(QVariantList gages READ gages NOTIFY currentProjectChanged)
    Q_PROPERTY(QVariantList grids READ grids NOTIFY gridsChanged)
    Q_PROPERTY(int gridsRevision READ gridsRevision NOTIFY gridsChanged)
    Q_PROPERTY(QVariantMap multipliers READ multipliers NOTIFY currentProjectChanged)
    Q_PROPERTY(QVariantMap comboGages READ comboGages NOTIFY currentProjectChanged)
    Q_PROPERTY(int availableCells READ availableCells NOTIFY currentProjectChanged)
    Q_PROPERTY(int minCases READ minCases NOTIFY currentProjectChanged)
    Q_PROPERTY(bool online READ online NOTIFY onlineChanged)
    Q_PROPERTY(int pendingChanges READ pendingChanges NOTIFY pendingChangesChanged)
    Q_PROPERTY(app::Updater* updater READ updater CONSTANT)

public:
    explicit AppController(QObject* parent = nullptr);
    ~AppController() override;

    bool init();

    ProjectModel* projects() const { return m_projectModel.get(); }

    QString currentProjectId() const;
    QString title() const;
    QString description() const;
    int gridSize() const;
    int startHP() const;
    bool freeCenter() const;
    bool gageMode() const;
    bool gridsDirty() const { return m_gridsDirty; }
    int lastTab() const { return m_lastTab; }

    QVariantList players() const;
    QVariantList cases() const;
    QVariantList gages() const;
    QVariantList grids() const;
    int gridsRevision() const { return m_gridsRevision; }
    QVariantMap multipliers() const;
    QVariantMap comboGages() const;
    int availableCells() const;
    int minCases() const;
    bool online() const;
    int pendingChanges() const;
    Updater* updater() const { return m_updater.get(); }

    void setTitle(const QString& v);
    void setDescription(const QString& v);
    void setGridSize(int v);
    void setStartHP(int v);
    void setFreeCenter(bool v);
    void setGageMode(bool v);
    void setLastTab(int v);

    Q_INVOKABLE void reloadProjects();
    Q_INVOKABLE QString createProject();
    Q_INVOKABLE bool openProject(const QString& id);
    Q_INVOKABLE QString cloneProject(const QString& id);
    Q_INVOKABLE void deleteProject(const QString& id);
    Q_INVOKABLE void updateProjectMeta(const QString& id, const QString& title,
                                       const QString& description);
    Q_INVOKABLE void saveCurrentProject();
    Q_INVOKABLE void saveConfig();
    Q_INVOKABLE void scheduleAutoSave();

    Q_INVOKABLE void setPlayerName(int index, const QString& name);
    Q_INVOKABLE void addPlayer();
    Q_INVOKABLE void removePlayer(int index);

    Q_INVOKABLE void addCase(const QString& label, int points, int rate);
    Q_INVOKABLE void updateCase(int index, const QString& label, int points, int rate);
    Q_INVOKABLE void removeCase(int index);

    Q_INVOKABLE void addGage(const QString& description, int hp);
    Q_INVOKABLE void updateGage(int index, const QString& description, int hp);
    Q_INVOKABLE void removeGage(int index);

    Q_INVOKABLE void setMultiplier(const QString& key, int value);
    Q_INVOKABLE void setComboGage(const QString& key, const QString& value);

    Q_INVOKABLE QString generateAll();
    Q_INVOKABLE QString seedDemoProject();
    Q_INVOKABLE void reshuffleGrid(int playerIdx);
    Q_INVOKABLE void swapGridCells(int playerIdx, int r1, int c1, int r2, int c2);
    Q_INVOKABLE void replaceGridCell(int playerIdx, int row, int col, int caseIdx);
    // Modifie le libellé (et optionnellement les points) d'une case déjà placée.
    Q_INVOKABLE void setGridCellLabel(int playerIdx, int row, int col,
                                      const QString& label, int points = -1);
    Q_INVOKABLE void moveGrid(int fromIdx, int toIdx);
    // Assigne une grille à un joueur (échange si le nom est déjà pris).
    Q_INVOKABLE void assignGridToPlayer(int gridIdx, const QString& playerName);

    Q_INVOKABLE bool exportCurrentJson(const QString& filePath);
    Q_INVOKABLE bool importJsonFile(const QString& filePath);
    Q_INVOKABLE bool exportProjectJson(const QString& id, const QString& filePath);
    Q_INVOKABLE bool exportAllJson(const QString& filePath);
    Q_INVOKABLE int importAllJsonFile(const QString& filePath);

    Q_INVOKABLE void pickExportCurrentJson();
    Q_INVOKABLE void pickExportProjectJson();
    Q_INVOKABLE void pickExportAllJson();
    Q_INVOKABLE void pickImportJson();
    Q_INVOKABLE void pickImportAllJson();

    Q_INVOKABLE QString buildShareUrl();
    Q_INVOKABLE bool importSharePayload(const QString& payload);
    Q_INVOKABLE void enableProjectSharing();
    Q_INVOKABLE bool joinProjectUri(const QString& uri);
    Q_INVOKABLE void handleJoinUrl(const QUrl& url);

    Q_INVOKABLE QVariantList loadPlayChecks(const QString& playerName);
    Q_INVOKABLE void savePlayChecks(const QString& playerName, const QVariantList& checks);
    // Cochage style Colo Courses/Tâches : source de vérité locale, vibration côté UI.
    // Coche/décoche (row,col) puis propage le même libellé à toutes les grilles
    // (tout le monde regarde le même film). Renvoie checks + overlays gages à enfiler.
    Q_INVOKABLE QVariantMap togglePlayCell(const QString& playerName, int row, int col);
    Q_INVOKABLE void resetPlayChecks(const QString& playerName);
    Q_INVOKABLE int computeScore(const QString& playerName, const QVariantList& checks);
    Q_INVOKABLE QVariantList detectBingoLines(const QVariantList& checks);

    Q_INVOKABLE void setKeepScreenOn(bool on);
    Q_INVOKABLE void lockLandscape();
    Q_INVOKABLE void unlockOrientation();
    Q_INVOKABLE void setImmersive(bool on);
    Q_INVOKABLE void vibrate();
    Q_INVOKABLE void copyToClipboard(const QString& text);
    Q_INVOKABLE QString detectLineType(const QVariantList& lineCoords, int gridSize) const;
    Q_INVOKABLE bool shareText(const QString& text);
    // Impression A4 : aperçu + service d'impression (2 grilles/page, gages, points).
    Q_INVOKABLE bool exportPdf(const QString& filePath);
    Q_INVOKABLE bool savePdf();
    Q_INVOKABLE bool printGrids();
    // Alias UI historique.
    Q_INVOKABLE bool printPreview() { return printGrids(); }
    Q_INVOKABLE bool saveScreenshot(const QString& filePath);
    Q_INVOKABLE void notify(const QString& message);

    Q_INVOKABLE QString formatRelativeDate(qint64 ms) const;

    static QString databasePath();

signals:
    void currentProjectChanged();
    void gridsChanged();
    void gridsDirtyChanged();
    void lastTabChanged();
    void onlineChanged();
    void pendingChangesChanged();
    void editorOpened(const QString& projectId);
    void toast(const QString& message);
    void playChecksChanged();

private:
    void touchProject();
    void persistCurrent();
    void markGridsDirty();
    void clearGridsDirtyFlag();
    core::Project* current();
    const core::Project* current() const;
    QVariantList gridsToVariant() const;

    std::unique_ptr<store::Database> m_db;
    std::unique_ptr<ProjectModel>    m_projectModel;
    std::unique_ptr<net::RelayPool>  m_relayPool;
    std::unique_ptr<ProjectSync>     m_projectSync;
    std::unique_ptr<Updater>         m_updater;
    core::Project                    m_current;
    bool                             m_hasCurrent = false;
    bool                             m_gridsDirty = false;
    int                              m_gridsRevision = 0;
    int                              m_lastTab     = 0;
    QTimer                           m_autoSaveTimer;
};

} // namespace app
