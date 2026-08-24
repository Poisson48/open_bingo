#include "opensubtitles_org.h"

#include <QDir>
#include <QFile>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

#ifndef BINGO_APP_VERSION
#  define BINGO_APP_VERSION "0.0.0"
#endif

namespace app {

namespace {

constexpr const char* kOrgSearchBase = "https://www.opensubtitles.org/en/search";
constexpr const char* kOrgDownloadBase = "https://dl.opensubtitles.org/en/download/sub/";
constexpr qint64 kOrgMinIntervalMs = 1500;

QString userAgent()
{
    return QStringLiteral("OpenBingo v") + QStringLiteral(BINGO_APP_VERSION);
}

} // namespace

OpenSubtitlesOrg::OpenSubtitlesOrg(QObject* parent)
    : QObject(parent)
{
}

bool OpenSubtitlesOrg::isKnownFileId(qint64 fileId) const
{
    return m_orgFileIds.contains(fileId);
}

QNetworkRequest OpenSubtitlesOrg::makeOrgRequest(const QUrl& url) const
{
    QNetworkRequest req{url};
    req.setRawHeader("User-Agent", userAgent().toUtf8());
    req.setRawHeader("Accept", "text/html,application/xhtml+xml;q=0.9,*/*;q=0.8");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    return req;
}

QString OpenSubtitlesOrg::langToIso639_2(const QString& lang)
{
    const QString code = lang.trimmed().toLower();
    if (code == QLatin1String("fr") || code == QLatin1String("fre") || code == QLatin1String("fra"))
        return QStringLiteral("fre");
    if (code == QLatin1String("en") || code == QLatin1String("eng"))
        return QStringLiteral("eng");
    // Autres codes ISO-639-1 non mappés → eng (plan)
    if (code.size() == 3)
        return code;
    return QStringLiteral("eng");
}

QString OpenSubtitlesOrg::encodeMovieName(const QString& query)
{
    QString s = query.trimmed();
    s.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral("+"));
    // Garder alphanumériques, +, -, ., _ ; encoder le reste
    QByteArray out;
    const QByteArray utf = s.toUtf8();
    for (unsigned char c : utf) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
            || c == '+' || c == '-' || c == '.' || c == '_') {
            out.append(static_cast<char>(c));
        } else {
            out.append('%');
            out.append(QByteArray::number(c, 16).rightJustified(2, '0').toUpper());
        }
    }
    return QString::fromLatin1(out);
}

QVariantList OpenSubtitlesOrg::parseOrgSearchHtml(const QByteArray& html,
                                                  const QString& language)
{
    QVariantList results;
    QSet<qint64> seen;

    const QString text = QString::fromUtf8(html);
    // /subtitles/ID-slug  |  /en/subtitles/ID-slug  |  /en/subtitleserve/sub/ID
    static const QRegularExpression re(
        QStringLiteral(
            R"((?:/en)?/subtitles/(\d+)(-[^"'<\s]*)?|/en/subtitleserve/sub/(\d+))"),
        QRegularExpression::CaseInsensitiveOption);

    auto addRow = [&](qint64 id, const QString& slug) {
        if (id <= 0 || seen.contains(id))
            return;
        seen.insert(id);
        QString title = slug;
        title.replace(QLatin1Char('-'), QLatin1Char(' '));
        title = title.trimmed();
        if (title.isEmpty())
            title = QStringLiteral("Subtitle #%1").arg(id);

        QVariantMap row;
        row.insert(QStringLiteral("fileId"), id);
        row.insert(QStringLiteral("title"), title);
        row.insert(QStringLiteral("year"), 0);
        row.insert(QStringLiteral("language"), language);
        row.insert(QStringLiteral("hearingImpaired"), false);
        row.insert(QStringLiteral("downloadCount"), 0);
        row.insert(QStringLiteral("release"), QString());
        row.insert(QStringLiteral("fps"), 0.0);
        row.insert(QStringLiteral("featureId"), 0);
        row.insert(QStringLiteral("fileName"), QString());
        row.insert(QStringLiteral("subtitleId"), id);
        row.insert(QStringLiteral("source"), QStringLiteral("org"));
        results.append(row);
    };

    auto it = re.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        if (m.captured(1).size()) {
            const qint64 id = m.captured(1).toLongLong();
            QString slug = m.captured(2);
            if (slug.startsWith(QLatin1Char('-')))
                slug = slug.mid(1);
            addRow(id, slug);
        } else if (m.captured(3).size()) {
            addRow(m.captured(3).toLongLong(), QString());
        }
    }
    return results;
}

void OpenSubtitlesOrg::runAfterOrgRateLimit(const std::function<void()>& fn)
{
    qint64 waitMs = 0;
    if (m_lastOrgRequest.isValid()) {
        const qint64 elapsed = m_lastOrgRequest.msecsTo(QDateTime::currentDateTimeUtc());
        if (elapsed >= 0 && elapsed < kOrgMinIntervalMs)
            waitMs = kOrgMinIntervalMs - elapsed;
    }
    auto run = [this, fn]() {
        m_lastOrgRequest = QDateTime::currentDateTimeUtc();
        fn();
    };
    if (waitMs <= 0) {
        run();
        return;
    }
    QTimer::singleShot(static_cast<int>(waitMs), this, run);
}

void OpenSubtitlesOrg::search(const QString& query, const QString& languages)
{
    const QString iso = langToIso639_2(languages);
    const QString movie = encodeMovieName(query);
    if (movie.isEmpty()) {
        emit searchFailed(QStringLiteral("Titre du film vide"));
        return;
    }

    const QUrl url(QString::fromLatin1(kOrgSearchBase)
                   + QStringLiteral("/sublanguageid-") + iso
                   + QStringLiteral("/moviename-") + movie);

    runAfterOrgRateLimit([this, url, languages]() {
        QNetworkReply* reply = m_nam.get(makeOrgRequest(url));
        connect(reply, &QNetworkReply::finished, this, [this, reply, languages]() {
            reply->deleteLater();

            if (reply->error() != QNetworkReply::NoError) {
                emit searchFailed(
                    QStringLiteral("Source .org indisponible, utilise .com / login : %1")
                        .arg(reply->errorString()));
                return;
            }

            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (status < 200 || status >= 300) {
                emit searchFailed(
                    QStringLiteral("Source .org indisponible, utilise .com / login (HTTP %1).")
                        .arg(status));
                return;
            }

            const QByteArray raw = reply->readAll();
            QVariantList results = parseOrgSearchHtml(raw, languages.trimmed());
            m_orgFileIds.clear();
            for (const QVariant& v : results) {
                const qint64 id = v.toMap().value(QStringLiteral("fileId")).toLongLong();
                if (id > 0)
                    m_orgFileIds.insert(id);
            }

            if (results.isEmpty()) {
                emit searchFailed(
                    QStringLiteral("Source .org indisponible, utilise .com / login "
                                   "(aucun résultat)."));
                return;
            }

            emit searchFinished(results);
        });
    });
}

void OpenSubtitlesOrg::download(qint64 fileId)
{
    const QUrl url(QString::fromLatin1(kOrgDownloadBase) + QString::number(fileId));

    runAfterOrgRateLimit([this, url, fileId]() {
        QNetworkReply* reply = m_nam.get(makeOrgRequest(url));
        connect(reply, &QNetworkReply::finished, this, [this, reply, fileId]() {
            reply->deleteLater();

            if (reply->error() != QNetworkReply::NoError) {
                emit downloadFailed(
                    QStringLiteral("Source .org indisponible, utilise .com / login : %1")
                        .arg(reply->errorString()));
                return;
            }

            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (status < 200 || status >= 300) {
                emit downloadFailed(
                    QStringLiteral("Source .org indisponible, utilise .com / login (HTTP %1).")
                        .arg(status));
                return;
            }

            const QByteArray body = reply->readAll();
            if (body.isEmpty()) {
                emit downloadFailed(QStringLiteral("Fichier sous-titres vide"));
                return;
            }

            // Parfois .org renvoie une page HTML au lieu du SRT
            const QByteArray trimmed = body.trimmed().left(64).toLower();
            if (trimmed.startsWith("<!doctype") || trimmed.startsWith("<html")) {
                emit downloadFailed(
                    QStringLiteral("Source .org indisponible, utilise .com / login "
                                   "(réponse HTML)."));
                return;
            }

            QVariantMap meta;
            meta.insert(QStringLiteral("fileId"), fileId);
            meta.insert(QStringLiteral("fileName"),
                        QStringLiteral("%1.srt").arg(fileId));
            meta.insert(QStringLiteral("source"), QStringLiteral("org"));
            meta.insert(QStringLiteral("featureId"), 0);

            const QString base =
                QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
            const QString dirPath = base + QStringLiteral("/subtitles");
            if (!QDir().mkpath(dirPath)) {
                emit downloadFailed(
                    QStringLiteral("Impossible de créer le dossier cache sous-titres"));
                return;
            }

            const QString localPath =
                dirPath + QLatin1Char('/')
                + QStringLiteral("0_%1_%2.srt").arg(fileId).arg(fileId);

            QFile out(localPath);
            if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                emit downloadFailed(QStringLiteral("Impossible d’écrire le fichier sous-titres"));
                return;
            }
            const qint64 written = out.write(body);
            out.close();
            if (written != body.size()) {
                QFile::remove(localPath);
                emit downloadFailed(QStringLiteral("Écriture incomplète du fichier sous-titres"));
                return;
            }

            meta.insert(QStringLiteral("localPath"), localPath);
            meta.insert(QStringLiteral("bytes"), static_cast<qint64>(body.size()));
            emit downloadFinished(localPath, meta);
        });
    });
}

} // namespace app
