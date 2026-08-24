#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

namespace app {

class AppController;
class OpenSubtitlesClient;

// État UI « Bingo film » — desktop. Plan: docs/PLAN-opensubtitles.md
class FilmAssistant : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(QString source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QVariantList results READ results NOTIFY resultsChanged)
    Q_PROPERTY(QVariantList cues READ cues NOTIFY cuesChanged)
    Q_PROPERTY(QString selectedPath READ selectedPath NOTIFY cuesChanged)
    Q_PROPERTY(bool hasApiKey READ hasApiKey NOTIFY apiKeyChanged)
    Q_PROPERTY(QString apiKey READ apiKey WRITE setApiKey NOTIFY apiKeyChanged)
    Q_PROPERTY(QString username READ username WRITE setUsername NOTIFY usernameChanged)
    Q_PROPERTY(bool canSearch READ canSearch NOTIFY sourceChanged)
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY loggedInChanged)

public:
    explicit FilmAssistant(AppController* controller, QObject* parent = nullptr);

    QString query() const { return m_query; }
    QString language() const { return m_language; }
    QString source() const;
    bool busy() const { return m_busy; }
    QString error() const { return m_error; }
    QVariantList results() const { return m_results; }
    QVariantList cues() const { return m_cues; }
    QString selectedPath() const { return m_selectedPath; }
    bool hasApiKey() const;
    QString apiKey() const;
    QString username() const { return m_username; }
    bool canSearch() const;
    bool loggedIn() const;

    void setQuery(const QString& q);
    void setLanguage(const QString& lang);
    void setSource(const QString& source);
    void setApiKey(const QString& key);
    void setUsername(const QString& user);

    Q_INVOKABLE void search();
    Q_INVOKABLE void downloadAndPreview(qint64 fileId);
    // indices vides → cues avec selected=true ; sinon indices CueList.
    Q_INVOKABLE QVariantMap importSelectedCues(bool skipSfx, int maxLen,
                                               const QVariantList& indices = {});
    Q_INVOKABLE void clearResults();
    Q_INVOKABLE void clearError();
    Q_INVOKABLE void login(const QString& password);

signals:
    void queryChanged();
    void languageChanged();
    void sourceChanged();
    void busyChanged();
    void errorChanged();
    void resultsChanged();
    void cuesChanged();
    void apiKeyChanged();
    void usernameChanged();
    void loggedInChanged();

private:
    void setBusy(bool busy);
    void setErrorMessage(const QString& message);
    void loadSettings();
    void onSearchFinished(const QVariantList& results);
    void onSearchFailed(const QString& message);
    void onDownloadFinished(const QString& localPath, const QVariantMap& meta);
    void onDownloadFailed(const QString& message);
    void onLoginFinished(const QVariantMap& info);
    void onLoginFailed(const QString& message);

    AppController* m_controller = nullptr;
    OpenSubtitlesClient* m_client = nullptr;
    QString m_query;
    QString m_language; // libre, mémorisé settings os_default_lang
    QString m_username;
    bool m_busy = false;
    QString m_error;
    QVariantList m_results;
    QVariantList m_cues;
    QString m_selectedPath;
};

} // namespace app
