#pragma once

#include "../core/bingotypes.h"
#include "../store/database.h"
#include "../net/relaypool.h"
#include "projectmodel.h"
#include "projectsync.h"
#include "updater.h"
#include "mcpserver.h"
#include "filmassistant.h"

#include <QObject>
#include <QImage>
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
    Q_PROPERTY(int gridRows READ gridRows WRITE setGridRows NOTIFY currentProjectChanged)
    Q_PROPERTY(int gridCols READ gridCols WRITE setGridCols NOTIFY currentProjectChanged)
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
    Q_PROPERTY(QString relayUrls READ relayUrls NOTIFY relayUrlsChanged)
    Q_PROPERTY(bool pushEnabled READ pushEnabled NOTIFY pushSettingsChanged)
    Q_PROPERTY(QString pushBaseUrl READ pushBaseUrl NOTIFY pushSettingsChanged)
    Q_PROPERTY(app::Updater* updater READ updater CONSTANT)
    Q_PROPERTY(app::McpServer* mcp READ mcp CONSTANT)
    Q_PROPERTY(app::FilmAssistant* film READ film CONSTANT)
    // Aperçu PNG classement (file://…?r=) — rafraîchi par prepareScoreboardPreview.
    Q_PROPERTY(QString scoreboardPreviewUrl READ scoreboardPreviewUrl NOTIFY scoreboardPreviewChanged)
    Q_PROPERTY(int scoreboardPreviewRevision READ scoreboardPreviewRevision NOTIFY scoreboardPreviewChanged)
    Q_PROPERTY(QString scoreboardShareLabel READ scoreboardShareLabel CONSTANT)

public:
    explicit AppController(QObject* parent = nullptr);
    ~AppController() override;

    bool init();

    ProjectModel* projects() const { return m_projectModel.get(); }

    QString currentProjectId() const;
    QString title() const;
    QString description() const;
    int gridSize() const;
    int gridRows() const;
    int gridCols() const;
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
    QString relayUrls() const;
    bool pushEnabled() const;
    QString pushBaseUrl() const;
    Updater* updater() const { return m_updater.get(); }
    McpServer* mcp() const { return m_mcp.get(); }
    FilmAssistant* film() const { return m_film.get(); }

    void setTitle(const QString& v);
    void setDescription(const QString& v);
    void setGridSize(int v);
    void setGridRows(int v);
    void setGridCols(int v);
    void setStartHP(int v);
    void setFreeCenter(bool v);
    void setGageMode(bool v);
    void setLastTab(int v);

    Q_INVOKABLE void reloadProjects();
    Q_INVOKABLE QString createProject();
    Q_INVOKABLE bool openProject(const QString& id, bool toPlay = true);
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

    Q_INVOKABLE void addGage(const QString& description, int hp, int number = 1, int rate = 100);
    Q_INVOKABLE void updateGage(int index, const QString& description, int hp,
                                int number = 1, int rate = 100);
    Q_INVOKABLE void removeGage(int index);
    // Plus grand n° de gage défini (pour le SpinBox Phrases).
    Q_INVOKABLE int maxGageNumber() const;

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

    // CSV phrases / gages (deux fichiers). replace=true remplace la liste.
    // Import → gridsDirty, grilles non vidées. Voir docs/PLAN-csv-mcp.md.
    Q_INVOKABLE QVariantMap importPhrasesCsvText(const QString& text, bool replace);
    Q_INVOKABLE QVariantMap importGagesCsvText(const QString& text, bool replace);
    Q_INVOKABLE QString exportPhrasesCsvText() const;
    Q_INVOKABLE QString exportGagesCsvText() const;
    Q_INVOKABLE void pickImportPhrasesCsv(bool replace);
    Q_INVOKABLE void pickImportGagesCsv(bool replace);
    Q_INVOKABLE void pickExportPhrasesCsv();
    Q_INVOKABLE void pickExportGagesCsv();

    Q_INVOKABLE QString buildShareUrl();
    Q_INVOKABLE QString joinUriForProject(const QString& projectId);
    Q_INVOKABLE bool importSharePayload(const QString& payload);
    Q_INVOKABLE void enableProjectSharing();
    Q_INVOKABLE bool joinProjectUri(const QString& uri);
    Q_INVOKABLE void handleJoinUrl(const QUrl& url);
    // Comme Colo « Quitter la liste » : retire le projet de cet appareil.
    Q_INVOKABLE void leaveProject(const QString& projectId);
    Q_INVOKABLE bool isProjectShared(const QString& projectId) const;

    Q_INVOKABLE void setRelayUrls(const QString& text);
    Q_INVOKABLE void resetRelayUrls();
    Q_INVOKABLE QString defaultRelayUrls() const;
    Q_INVOKABLE void setPushSettings(bool enabled, const QString& baseUrl);
    Q_INVOKABLE QString defaultPushBaseUrl() const;
    Q_INVOKABLE void onApplicationStateChanged(Qt::ApplicationState state);

    Q_INVOKABLE QVariantList loadPlayChecks(const QString& playerName);
    Q_INVOKABLE void savePlayChecks(const QString& playerName, const QVariantList& checks);
    // Cochage style Colo Courses/Tâches : source de vérité locale, vibration côté UI.
    // Coche/décoche (row,col) puis propage le même libellé à toutes les grilles
    // (tout le monde regarde le même film). Renvoie checks + overlays gages à enfiler.
    Q_INVOKABLE QVariantMap togglePlayCell(const QString& playerName, int row, int col);
    // Remet à zéro les coches de TOUTES les grilles (libellés partagés).
    // L'argument est ignoré (conservé pour compat QML).
    Q_INVOKABLE void resetPlayChecks(const QString& playerName);
    Q_INVOKABLE int computeScore(const QString& playerName, const QVariantList& checks);
    Q_INVOKABLE QVariantList detectBingoLines(const QVariantList& checks);
    // Classement live (tous joueurs) — rafraîchi via playChecksChanged.
    Q_INVOKABLE QVariantList playScoreboard() const;
    Q_INVOKABLE bool exportScoreboardPng(const QString& filePath);
    // Génère le PNG en cache et renvoie l'URL pour Image QML.
    Q_INVOKABLE QString prepareScoreboardPreview();
    // Android : feuille de partage ; desktop : dialogue Enregistrer.
    Q_INVOKABLE bool shareScoreboardPng();
    // Alias historique → shareScoreboardPng.
    Q_INVOKABLE bool saveScoreboardPng();
    QString scoreboardPreviewUrl() const;
    int scoreboardPreviewRevision() const { return m_scoreboardPreviewRevision; }
    QString scoreboardShareLabel() const;

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

    // Settings SQLite (clé API OpenSubtitles, langue, etc.) — jamais sync Nostr.
    Q_INVOKABLE QString settingsGet(const QString& key,
                                    const QString& defaultValue = QString()) const;
    Q_INVOKABLE void settingsSet(const QString& key, const QString& value);

    static QString databasePath();

signals:
    void currentProjectChanged();
    void gridsChanged();
    void gridsDirtyChanged();
    void lastTabChanged();
    void onlineChanged();
    void pendingChangesChanged();
    void relayUrlsChanged();
    void pushSettingsChanged();
    void editorOpened(const QString& projectId);
    void toast(const QString& message);
    void playChecksChanged();
    // Gages à afficher après un cochage distant (sync) — même file que togglePlayCell.
    void playOverlaysTriggered(const QVariantList& overlays);
    // Nouveau(x) gagnant(s) : toast + panneau (local et sync distant).
    void playWinnersTriggered(const QVariantList& winners, const QVariantList& scoreboard);
    void scoreboardPreviewChanged();

private:
    void touchProject();
    void persistCurrent();
    // Bump updatedAt + publish snapshot si le projet est partagé (coches Play).
    void publishPlayChecksIfShared();
    void markGridsDirty();
    void clearGridsDirtyFlag();
    void rememberPlayChecksSnapshot();
    void announceWinners(const QVariantList& newWinners, const QVariantList& scoreboard);
    core::Project* current();
    const core::Project* current() const;
    QVariantList gridsToVariant() const;
    QImage renderScoreboardImage();
    void refreshPushTopics();

    std::unique_ptr<store::Database> m_db;
    std::unique_ptr<ProjectModel>    m_projectModel;
    std::unique_ptr<net::RelayPool>  m_relayPool;
    std::unique_ptr<ProjectSync>     m_projectSync;
    std::unique_ptr<Updater>         m_updater;
    std::unique_ptr<McpServer>       m_mcp;
    std::unique_ptr<FilmAssistant>   m_film;
    core::Project                    m_current;
    bool                             m_hasCurrent = false;
    bool                             m_gridsDirty = false;
    int                              m_gridsRevision = 0;
    int                              m_lastTab     = 0;
    QTimer                           m_autoSaveTimer;
    QString                          m_deviceId;
    bool                             m_pushLifecycleReady = false;
    // Coches connues avant le dernier merge distant — pour déclencher les gages.
    std::map<std::string, std::string> m_playChecksSnapshot;
    QString                          m_scoreboardPreviewPath;
    int                              m_scoreboardPreviewRevision = 0;
};

} // namespace app
