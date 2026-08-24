#pragma once

#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <functional>

class QJsonValue;
class QUrl;

namespace app {

// Client OpenSubtitles — .com API v1 + fallback .org HTML (best-effort).
// Plan: docs/PLAN-opensubtitles.md § Providers
// Jamais de clé API hardcodée.
class OpenSubtitlesClient : public QObject
{
    Q_OBJECT
public:
    enum class Source { Auto, Com, Org };

    explicit OpenSubtitlesClient(QObject* parent = nullptr);

    void setApiKey(const QString& key);
    QString apiKey() const { return m_apiKey; }

    void setJwt(const QString& token);
    QString jwt() const { return m_jwt; }

    // "auto" | "com" | "org"
    void setSource(const QString& source);
    QString source() const;
    Source sourceEnum() const { return m_source; }

    // Async — emits searchFinished / searchFailed
    Q_INVOKABLE void search(const QString& query, const QString& languages,
                            const QString& hearingImpaired = QStringLiteral("include"));

    // Async — emits downloadFinished(path, meta) / downloadFailed
    // Si source du résultat = org (ou fileId connu org) → dl.opensubtitles.org
    Q_INVOKABLE void download(qint64 fileId);

    // Login free JWT optionnel — POST /api/v1/login (Api-Key requise)
    Q_INVOKABLE void login(const QString& username, const QString& password);

signals:
    void searchFinished(const QVariantList& results);
    void searchFailed(const QString& message);
    void downloadFinished(const QString& localPath, const QVariantMap& meta);
    void downloadFailed(const QString& message);
    void loginFinished(const QVariantMap& info);
    void loginFailed(const QString& message);

private:
    QNetworkRequest makeApiRequest(const QUrl& url) const;
    QNetworkRequest makeOrgRequest(const QUrl& url) const;
    static QString mapHttpError(int status, const QString& body, const QString& fallback);
    static bool jsonTruthy(const QJsonValue& v);
    static QString langToIso639_2(const QString& lang);
    static QString encodeMovieName(const QString& query);
    static QVariantList parseOrgSearchHtml(const QByteArray& html, const QString& language);

    void searchCom(const QString& query, const QString& languages,
                   const QString& hearingImpaired, bool fallbackToOrg);
    void searchOrgWeb(const QString& query, const QString& languages);
    void downloadCom(qint64 fileId);
    void downloadOrg(qint64 fileId);
    void fetchSubtitleFile(qint64 fileId, const QUrl& link, QVariantMap meta);
    void runAfterOrgRateLimit(const std::function<void()>& fn);
    bool isOrgFileId(qint64 fileId) const;

    QString m_apiKey;
    QString m_jwt;
    Source m_source = Source::Auto;
    QNetworkAccessManager m_nam;
    QSet<qint64> m_orgFileIds;
    QDateTime m_lastOrgRequest; // UTC
};

} // namespace app
