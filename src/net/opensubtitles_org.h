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

class QUrl;

namespace app {

// Scrape HTML opensubtitles.org + rate-limit (best-effort, sans clé API).
class OpenSubtitlesOrg : public QObject
{
    Q_OBJECT
public:
    explicit OpenSubtitlesOrg(QObject* parent = nullptr);

    void search(const QString& query, const QString& languages);
    void download(qint64 fileId);

    bool isKnownFileId(qint64 fileId) const;

signals:
    void searchFinished(const QVariantList& results);
    void searchFailed(const QString& message);
    void downloadFinished(const QString& localPath, const QVariantMap& meta);
    void downloadFailed(const QString& message);

private:
    QNetworkRequest makeOrgRequest(const QUrl& url) const;
    static QString langToIso639_2(const QString& lang);
    static QString encodeMovieName(const QString& query);
    static QVariantList parseOrgSearchHtml(const QByteArray& html, const QString& language);
    void runAfterOrgRateLimit(const std::function<void()>& fn);

    QNetworkAccessManager m_nam;
    QSet<qint64> m_orgFileIds;
    QDateTime m_lastOrgRequest; // UTC
};

} // namespace app
