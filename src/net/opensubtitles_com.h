#pragma once

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class QJsonValue;
class QUrl;

namespace app {

// REST client pour api.opensubtitles.com/api/v1 (search / download / login).
class OpenSubtitlesCom : public QObject
{
    Q_OBJECT
public:
    explicit OpenSubtitlesCom(QObject* parent = nullptr);

    void setApiKey(const QString& key);
    QString apiKey() const { return m_apiKey; }

    void setJwt(const QString& token);
    QString jwt() const { return m_jwt; }

    void search(const QString& query, const QString& languages,
                const QString& hearingImpaired = QStringLiteral("include"));
    void download(qint64 fileId);
    void login(const QString& username, const QString& password);

signals:
    void searchFinished(const QVariantList& results);
    void searchFailed(const QString& message);
    void downloadFinished(const QString& localPath, const QVariantMap& meta);
    void downloadFailed(const QString& message);
    void loginFinished(const QVariantMap& info);
    void loginFailed(const QString& message);

private:
    QNetworkRequest makeApiRequest(const QUrl& url) const;
    static QString mapHttpError(int status, const QString& body, const QString& fallback);
    static bool jsonTruthy(const QJsonValue& v);
    void fetchSubtitleFile(qint64 fileId, const QUrl& link, QVariantMap meta);

    QString m_apiKey;
    QString m_jwt;
    QNetworkAccessManager m_nam;
};

} // namespace app
