#include "opensubtitlesclient.h"

#include "opensubtitles_com.h"
#include "opensubtitles_org.h"

namespace app {

OpenSubtitlesClient::OpenSubtitlesClient(QObject* parent)
    : QObject(parent)
    , m_com(new OpenSubtitlesCom(this))
    , m_org(new OpenSubtitlesOrg(this))
{
    connect(m_com, &OpenSubtitlesCom::searchFinished, this,
            &OpenSubtitlesClient::onComSearchFinished);
    connect(m_com, &OpenSubtitlesCom::searchFailed, this, &OpenSubtitlesClient::onComSearchFailed);
    connect(m_com, &OpenSubtitlesCom::downloadFinished, this, &OpenSubtitlesClient::downloadFinished);
    connect(m_com, &OpenSubtitlesCom::downloadFailed, this, &OpenSubtitlesClient::downloadFailed);
    connect(m_com, &OpenSubtitlesCom::loginFinished, this, &OpenSubtitlesClient::loginFinished);
    connect(m_com, &OpenSubtitlesCom::loginFailed, this, &OpenSubtitlesClient::loginFailed);

    connect(m_org, &OpenSubtitlesOrg::searchFinished, this, &OpenSubtitlesClient::searchFinished);
    connect(m_org, &OpenSubtitlesOrg::searchFailed, this, &OpenSubtitlesClient::searchFailed);
    connect(m_org, &OpenSubtitlesOrg::downloadFinished, this, &OpenSubtitlesClient::downloadFinished);
    connect(m_org, &OpenSubtitlesOrg::downloadFailed, this, &OpenSubtitlesClient::downloadFailed);
}

void OpenSubtitlesClient::setApiKey(const QString& key)
{
    m_com->setApiKey(key);
}

QString OpenSubtitlesClient::apiKey() const
{
    return m_com->apiKey();
}

void OpenSubtitlesClient::setJwt(const QString& token)
{
    m_com->setJwt(token);
}

QString OpenSubtitlesClient::jwt() const
{
    return m_com->jwt();
}

void OpenSubtitlesClient::setSource(const QString& source)
{
    const QString s = source.trimmed().toLower();
    if (s == QLatin1String("com"))
        m_source = Source::Com;
    else if (s == QLatin1String("org"))
        m_source = Source::Org;
    else
        m_source = Source::Auto;
}

QString OpenSubtitlesClient::source() const
{
    switch (m_source) {
    case Source::Com:
        return QStringLiteral("com");
    case Source::Org:
        return QStringLiteral("org");
    case Source::Auto:
    default:
        return QStringLiteral("auto");
    }
}

void OpenSubtitlesClient::search(const QString& query, const QString& languages,
                                 const QString& hearingImpaired)
{
    const QString q = query.trimmed();
    if (q.isEmpty()) {
        emit searchFailed(QStringLiteral("Titre du film vide"));
        return;
    }

    m_autoFallback = false;
    m_pendingQuery.clear();
    m_pendingLanguages.clear();

    switch (m_source) {
    case Source::Com:
        m_com->search(q, languages, hearingImpaired);
        break;
    case Source::Org:
        m_org->search(q, languages);
        break;
    case Source::Auto:
    default:
        if (m_com->apiKey().isEmpty()) {
            m_org->search(q, languages);
        } else {
            m_autoFallback = true;
            m_pendingQuery = q;
            m_pendingLanguages = languages;
            m_com->search(q, languages, hearingImpaired);
        }
        break;
    }
}

void OpenSubtitlesClient::onComSearchFinished(const QVariantList& results)
{
    if (m_autoFallback && results.isEmpty()) {
        m_autoFallback = false;
        const QString q = m_pendingQuery;
        const QString lang = m_pendingLanguages;
        m_pendingQuery.clear();
        m_pendingLanguages.clear();
        m_org->search(q, lang);
        return;
    }
    m_autoFallback = false;
    m_pendingQuery.clear();
    m_pendingLanguages.clear();
    emit searchFinished(results);
}

void OpenSubtitlesClient::onComSearchFailed(const QString& message)
{
    if (m_autoFallback) {
        m_autoFallback = false;
        const QString q = m_pendingQuery;
        const QString lang = m_pendingLanguages;
        m_pendingQuery.clear();
        m_pendingLanguages.clear();
        m_org->search(q, lang);
        return;
    }
    emit searchFailed(message);
}

void OpenSubtitlesClient::download(qint64 fileId)
{
    if (fileId <= 0) {
        emit downloadFailed(QStringLiteral("file_id invalide"));
        return;
    }

    if (m_source == Source::Org || m_org->isKnownFileId(fileId)) {
        m_org->download(fileId);
        return;
    }
    m_com->download(fileId);
}

void OpenSubtitlesClient::login(const QString& username, const QString& password)
{
    m_com->login(username, password);
}

} // namespace app
