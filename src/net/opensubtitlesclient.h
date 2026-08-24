#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

namespace app {

class OpenSubtitlesCom;
class OpenSubtitlesOrg;

// Façade OpenSubtitles — délègue vers .com REST ou .org scrape selon source auto|com|org.
// Plan: docs/PLAN-opensubtitles.md § Providers
// Jamais de clé API hardcodée.
class OpenSubtitlesClient : public QObject
{
    Q_OBJECT
public:
    enum class Source { Auto, Com, Org };

    explicit OpenSubtitlesClient(QObject* parent = nullptr);

    void setApiKey(const QString& key);
    QString apiKey() const;

    void setJwt(const QString& token);
    QString jwt() const;

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
    void onComSearchFinished(const QVariantList& results);
    void onComSearchFailed(const QString& message);

    Source m_source = Source::Auto;
    OpenSubtitlesCom* m_com = nullptr;
    OpenSubtitlesOrg* m_org = nullptr;

    // Auto: fallback .org si .com échoue ou renvoie vide
    bool m_autoFallback = false;
    QString m_pendingQuery;
    QString m_pendingLanguages;
};

} // namespace app
