#include "filmassistant.h"
#include "appcontroller.h"
#include "../core/srtcodec.h"
#include "../net/opensubtitlesclient.h"

#include <QSet>

namespace app {

FilmAssistant::FilmAssistant(AppController* controller, QObject* parent)
    : QObject(parent)
    , m_controller(controller)
    , m_client(new OpenSubtitlesClient(this))
{
    connect(m_client, &OpenSubtitlesClient::searchFinished,
            this, &FilmAssistant::onSearchFinished);
    connect(m_client, &OpenSubtitlesClient::searchFailed,
            this, &FilmAssistant::onSearchFailed);
    connect(m_client, &OpenSubtitlesClient::downloadFinished,
            this, &FilmAssistant::onDownloadFinished);
    connect(m_client, &OpenSubtitlesClient::downloadFailed,
            this, &FilmAssistant::onDownloadFailed);
    connect(m_client, &OpenSubtitlesClient::loginFinished,
            this, &FilmAssistant::onLoginFinished);
    connect(m_client, &OpenSubtitlesClient::loginFailed,
            this, &FilmAssistant::onLoginFailed);

    loadSettings();
}

void FilmAssistant::loadSettings()
{
    if (!m_controller || !m_client)
        return;
    const QString key = m_controller->settingsGet(QStringLiteral("os_api_key"));
    if (!key.isEmpty()) {
        m_client->setApiKey(key);
        emit apiKeyChanged();
    }
    const QString jwt = m_controller->settingsGet(QStringLiteral("os_jwt"));
    if (!jwt.isEmpty())
        m_client->setJwt(jwt);

    const QString user = m_controller->settingsGet(QStringLiteral("os_username"));
    if (user != m_username) {
        m_username = user;
        emit usernameChanged();
    }

    const QString src = m_controller->settingsGet(QStringLiteral("os_source"),
                                                  QStringLiteral("auto"));
    m_client->setSource(src);
    emit sourceChanged();

    const QString lang = m_controller->settingsGet(QStringLiteral("os_default_lang"));
    if (lang.isEmpty()) {
        const QString legacy = m_controller->settingsGet(QStringLiteral("os_language"));
        if (!legacy.isEmpty() && legacy != m_language) {
            m_language = legacy;
            m_controller->settingsSet(QStringLiteral("os_default_lang"), m_language);
            emit languageChanged();
        }
    } else if (lang != m_language) {
        m_language = lang;
        emit languageChanged();
    }

    if (loggedIn())
        emit loggedInChanged();
}

bool FilmAssistant::hasApiKey() const
{
    return !apiKey().isEmpty();
}

QString FilmAssistant::apiKey() const
{
    return m_client ? m_client->apiKey() : QString();
}

QString FilmAssistant::source() const
{
    return m_client ? m_client->source() : QStringLiteral("auto");
}

bool FilmAssistant::canSearch() const
{
    if (!m_client)
        return false;
    const QString s = m_client->source();
    if (s == QLatin1String("org") || s == QLatin1String("auto"))
        return true;
    return hasApiKey();
}

bool FilmAssistant::loggedIn() const
{
    return m_client && !m_client->jwt().isEmpty();
}

void FilmAssistant::setQuery(const QString& q)
{
    if (m_query == q)
        return;
    m_query = q;
    emit queryChanged();
}

void FilmAssistant::setLanguage(const QString& lang)
{
    if (m_language == lang)
        return;
    m_language = lang;
    if (m_controller)
        m_controller->settingsSet(QStringLiteral("os_default_lang"), m_language);
    emit languageChanged();
}

void FilmAssistant::setSource(const QString& source)
{
    if (!m_client)
        return;
    const QString before = m_client->source();
    m_client->setSource(source);
    if (m_client->source() == before)
        return;
    if (m_controller)
        m_controller->settingsSet(QStringLiteral("os_source"), m_client->source());
    emit sourceChanged();
}

void FilmAssistant::setApiKey(const QString& key)
{
    if (!m_client)
        return;
    const QString before = m_client->apiKey();
    m_client->setApiKey(key);
    if (m_client->apiKey() == before)
        return;
    if (m_controller)
        m_controller->settingsSet(QStringLiteral("os_api_key"), m_client->apiKey());
    emit apiKeyChanged();
    emit sourceChanged(); // canSearch peut changer
}

void FilmAssistant::setUsername(const QString& user)
{
    const QString trimmed = user.trimmed();
    if (m_username == trimmed)
        return;
    m_username = trimmed;
    if (m_controller)
        m_controller->settingsSet(QStringLiteral("os_username"), m_username);
    emit usernameChanged();
}

void FilmAssistant::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}

void FilmAssistant::setErrorMessage(const QString& message)
{
    if (m_error == message)
        return;
    m_error = message;
    emit errorChanged();
}

void FilmAssistant::search()
{
    setErrorMessage(QString());
    if (!m_client) {
        setErrorMessage(QStringLiteral("Client OpenSubtitles indisponible"));
        return;
    }
    if (!canSearch()) {
        setErrorMessage(QStringLiteral(
            "Ajoute ta clé OpenSubtitles (gratuite) dans Réglages pour chercher des films."));
        return;
    }
    if (m_client->source() == QLatin1String("com") && m_client->apiKey().isEmpty()) {
        setErrorMessage(QStringLiteral(
            "Ajoute ta clé OpenSubtitles (gratuite) dans Réglages pour chercher des films."));
        return;
    }
    m_results.clear();
    emit resultsChanged();
    setBusy(true);
    m_client->search(m_query, m_language, QStringLiteral("include"));
}

void FilmAssistant::downloadAndPreview(qint64 fileId)
{
    setErrorMessage(QString());
    if (!m_client) {
        setErrorMessage(QStringLiteral("Client OpenSubtitles indisponible"));
        return;
    }
    if (fileId <= 0) {
        setErrorMessage(QStringLiteral("file_id invalide"));
        return;
    }
    m_cues.clear();
    m_selectedPath.clear();
    emit cuesChanged();
    setBusy(true);
    m_client->download(fileId);
}

void FilmAssistant::login(const QString& password)
{
    setErrorMessage(QString());
    if (!m_client) {
        setErrorMessage(QStringLiteral("Client OpenSubtitles indisponible"));
        return;
    }
    if (m_username.trimmed().isEmpty() || password.isEmpty()) {
        setErrorMessage(QStringLiteral("Identifiants OpenSubtitles incomplets"));
        return;
    }
    setBusy(true);
    m_client->login(m_username, password);
}

void FilmAssistant::onSearchFinished(const QVariantList& results)
{
    m_results = results;
    emit resultsChanged();
    setBusy(false);
}

void FilmAssistant::onSearchFailed(const QString& message)
{
    setBusy(false);
    setErrorMessage(message);
}

void FilmAssistant::onDownloadFinished(const QString& localPath, const QVariantMap& meta)
{
    Q_UNUSED(meta);
    m_selectedPath = localPath;
    m_cues.clear();

    const auto parsed = core::parseSrtFile(localPath.toStdString());
    for (const auto& cue : parsed.cues) {
        QVariantMap row;
        row.insert(QStringLiteral("startMs"), cue.startMs);
        row.insert(QStringLiteral("endMs"), cue.endMs);
        row.insert(QStringLiteral("plain"), QString::fromStdString(cue.plain));
        row.insert(QStringLiteral("text"), QString::fromStdString(cue.text));
        row.insert(QStringLiteral("likelySfx"), cue.likelySfx);
        row.insert(QStringLiteral("selected"), !cue.likelySfx);
        m_cues.append(row);
    }

    emit cuesChanged();
    setBusy(false);

    if (!parsed.errors.empty() && m_cues.isEmpty()) {
        QStringList errs;
        for (const auto& e : parsed.errors)
            errs << QString::fromStdString(e);
        setErrorMessage(errs.join(QStringLiteral("\n")));
    }
}

void FilmAssistant::onDownloadFailed(const QString& message)
{
    setBusy(false);
    setErrorMessage(message);
}

void FilmAssistant::onLoginFinished(const QVariantMap& info)
{
    setBusy(false);
    const QString token = info.value(QStringLiteral("token")).toString();
    if (m_controller && !token.isEmpty())
        m_controller->settingsSet(QStringLiteral("os_jwt"), token);
    emit loggedInChanged();
    setErrorMessage(QString());
}

void FilmAssistant::onLoginFailed(const QString& message)
{
    setBusy(false);
    setErrorMessage(message);
}

QVariantMap FilmAssistant::importSelectedCues(bool skipSfx, int maxLen,
                                              const QVariantList& indices)
{
    if (!m_controller || m_controller->currentProjectId().isEmpty()) {
        return QVariantMap{
            {QStringLiteral("ok"), false},
            {QStringLiteral("added"), 0},
        };
    }

    QSet<int> pick;
    for (const QVariant& v : indices) {
        bool ok = false;
        const int i = v.toInt(&ok);
        if (ok && i >= 0)
            pick.insert(i);
    }
    const bool useIndices = !indices.isEmpty();

    int added = 0;
    for (int i = 0; i < m_cues.size(); ++i) {
        const QVariantMap cue = m_cues[i].toMap();
        if (useIndices) {
            if (!pick.contains(i))
                continue;
        } else if (!cue.value(QStringLiteral("selected")).toBool()) {
            continue;
        }
        if (skipSfx && cue.value(QStringLiteral("likelySfx")).toBool())
            continue;
        const QString plain = cue.value(QStringLiteral("plain")).toString().trimmed();
        if (plain.isEmpty())
            continue;
        if (maxLen > 0 && plain.size() > maxLen)
            continue;
        m_controller->addCase(plain, 1, 50);
        ++added;
    }

    return QVariantMap{
        {QStringLiteral("ok"), true},
        {QStringLiteral("added"), added},
    };
}

void FilmAssistant::clearResults()
{
    m_results.clear();
    m_cues.clear();
    m_selectedPath.clear();
    emit resultsChanged();
    emit cuesChanged();
}

void FilmAssistant::clearError()
{
    setErrorMessage(QString());
}

} // namespace app
