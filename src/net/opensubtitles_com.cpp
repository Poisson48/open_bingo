#include "opensubtitles_com.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QUrl>
#include <QUrlQuery>

#include <algorithm>

#ifndef BINGO_APP_VERSION
#  define BINGO_APP_VERSION "0.0.0"
#endif

namespace app {

namespace {

constexpr const char* kApiBase = "https://api.opensubtitles.com/api/v1";

QString userAgent()
{
    return QStringLiteral("OpenBingo v") + QStringLiteral(BINGO_APP_VERSION);
}

} // namespace

OpenSubtitlesCom::OpenSubtitlesCom(QObject* parent)
    : QObject(parent)
{
}

void OpenSubtitlesCom::setApiKey(const QString& key)
{
    m_apiKey = key.trimmed();
}

void OpenSubtitlesCom::setJwt(const QString& token)
{
    m_jwt = token.trimmed();
}

QNetworkRequest OpenSubtitlesCom::makeApiRequest(const QUrl& url) const
{
    QNetworkRequest req{url};
    req.setRawHeader("Api-Key", m_apiKey.toUtf8());
    req.setRawHeader("User-Agent", userAgent().toUtf8());
    req.setRawHeader("Accept", "application/json");
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    if (!m_jwt.isEmpty())
        req.setRawHeader("Authorization",
                         QByteArray("Bearer ") + m_jwt.toUtf8());
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    return req;
}

bool OpenSubtitlesCom::jsonTruthy(const QJsonValue& v)
{
    if (v.isBool())
        return v.toBool();
    if (v.isDouble())
        return v.toInt() != 0;
    if (v.isString()) {
        const QString s = v.toString().trimmed().toLower();
        return s == QLatin1String("1") || s == QLatin1String("true") || s == QLatin1String("yes");
    }
    return false;
}

QString OpenSubtitlesCom::mapHttpError(int status, const QString& body, const QString& fallback)
{
    Q_UNUSED(body);
    switch (status) {
    case 401:
        return QStringLiteral("Clé API OpenSubtitles invalide ou absente (401).");
    case 403:
        return QStringLiteral("Accès refusé par OpenSubtitles (403). Vérifie ta clé API.");
    case 429:
        return QStringLiteral(
            "Plus de téléchargements aujourd’hui (OpenSubtitles). Réessaie demain ou "
            "connecte un compte.");
    default:
        break;
    }
    if (!fallback.isEmpty())
        return fallback;
    return QStringLiteral("Erreur OpenSubtitles (HTTP %1).").arg(status);
}

void OpenSubtitlesCom::search(const QString& query, const QString& languages,
                              const QString& hearingImpaired)
{
    if (m_apiKey.isEmpty()) {
        emit searchFailed(QStringLiteral("Clé API OpenSubtitles manquante"));
        return;
    }

    QUrl url(QString::fromLatin1(kApiBase) + QStringLiteral("/subtitles"));
    QUrlQuery params;
    params.addQueryItem(QStringLiteral("query"), query);
    params.addQueryItem(QStringLiteral("type"), QStringLiteral("movie"));
    const QString hi = hearingImpaired.trimmed().isEmpty()
                           ? QStringLiteral("include")
                           : hearingImpaired.trimmed();
    params.addQueryItem(QStringLiteral("hearing_impaired"), hi);
    const QString lang = languages.trimmed();
    if (!lang.isEmpty())
        params.addQueryItem(QStringLiteral("languages"), lang);
    url.setQuery(params);

    QNetworkReply* reply = m_nam.get(makeApiRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray raw = reply->readAll();

        if (reply->error() != QNetworkReply::NoError) {
            if (status == 401 || status == 403 || status == 429) {
                emit searchFailed(mapHttpError(status, QString::fromUtf8(raw), QString()));
                return;
            }
            emit searchFailed(QStringLiteral("Échec de la recherche OpenSubtitles : %1")
                                  .arg(reply->errorString()));
            return;
        }
        if (status == 401 || status == 403 || status == 429) {
            emit searchFailed(mapHttpError(status, QString::fromUtf8(raw), QString()));
            return;
        }
        if (status < 200 || status >= 300) {
            emit searchFailed(mapHttpError(status, QString::fromUtf8(raw),
                                           QStringLiteral("Recherche OpenSubtitles échouée.")));
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(raw);
        if (!doc.isObject()) {
            emit searchFailed(QStringLiteral("Réponse OpenSubtitles invalide"));
            return;
        }

        QVariantList results;
        const QJsonArray data = doc.object().value(QStringLiteral("data")).toArray();
        for (const QJsonValue& itemVal : data) {
            const QJsonObject item = itemVal.toObject();
            const QJsonObject attrs = item.value(QStringLiteral("attributes")).toObject();
            const QJsonObject feature =
                attrs.value(QStringLiteral("feature_details")).toObject();
            const bool hiFlag = jsonTruthy(attrs.value(QStringLiteral("hearing_impaired")));
            const int downloadCount = attrs.value(QStringLiteral("download_count")).toInt();
            const QString title = feature.value(QStringLiteral("title")).toString();
            const int year = feature.value(QStringLiteral("year")).toInt();
            const qint64 featureId =
                static_cast<qint64>(feature.value(QStringLiteral("feature_id")).toDouble());
            const QString language = attrs.value(QStringLiteral("language")).toString();
            const QString release = attrs.value(QStringLiteral("release")).toString();
            const double fps = attrs.value(QStringLiteral("fps")).toDouble();

            const QJsonArray files = attrs.value(QStringLiteral("files")).toArray();
            if (files.isEmpty())
                continue;

            for (const QJsonValue& fileVal : files) {
                const QJsonObject file = fileVal.toObject();
                const qint64 fileId =
                    static_cast<qint64>(file.value(QStringLiteral("file_id")).toDouble());
                if (fileId <= 0)
                    continue;

                QVariantMap row;
                row.insert(QStringLiteral("fileId"), fileId);
                row.insert(QStringLiteral("title"), title);
                row.insert(QStringLiteral("year"), year);
                row.insert(QStringLiteral("language"), language);
                row.insert(QStringLiteral("hearingImpaired"), hiFlag);
                row.insert(QStringLiteral("downloadCount"), downloadCount);
                row.insert(QStringLiteral("release"), release);
                row.insert(QStringLiteral("fps"), fps);
                row.insert(QStringLiteral("featureId"), featureId);
                row.insert(QStringLiteral("fileName"),
                           file.value(QStringLiteral("file_name")).toString());
                row.insert(QStringLiteral("subtitleId"),
                           attrs.value(QStringLiteral("subtitle_id")).toVariant());
                row.insert(QStringLiteral("source"), QStringLiteral("com"));
                results.append(row);
            }
        }

        std::sort(results.begin(), results.end(), [](const QVariant& a, const QVariant& b) {
            const QVariantMap ma = a.toMap();
            const QVariantMap mb = b.toMap();
            const bool hiA = ma.value(QStringLiteral("hearingImpaired")).toBool();
            const bool hiB = mb.value(QStringLiteral("hearingImpaired")).toBool();
            if (hiA != hiB)
                return hiA && !hiB;
            return ma.value(QStringLiteral("downloadCount")).toInt()
                 > mb.value(QStringLiteral("downloadCount")).toInt();
        });

        emit searchFinished(results);
    });
}

void OpenSubtitlesCom::download(qint64 fileId)
{
    if (m_apiKey.isEmpty()) {
        emit downloadFailed(QStringLiteral("Clé API OpenSubtitles manquante"));
        return;
    }

    const QUrl url(QString::fromLatin1(kApiBase) + QStringLiteral("/download"));
    QJsonObject body;
    body.insert(QStringLiteral("file_id"), fileId);
    const QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);

    QNetworkReply* reply = m_nam.post(makeApiRequest(url), payload);
    connect(reply, &QNetworkReply::finished, this, [this, reply, fileId]() {
        reply->deleteLater();

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray raw = reply->readAll();

        if (reply->error() != QNetworkReply::NoError) {
            if (status == 401 || status == 403 || status == 429) {
                emit downloadFailed(mapHttpError(status, QString::fromUtf8(raw), QString()));
                return;
            }
            emit downloadFailed(QStringLiteral("Échec du téléchargement OpenSubtitles : %1")
                                    .arg(reply->errorString()));
            return;
        }
        if (status == 401 || status == 403 || status == 429) {
            emit downloadFailed(mapHttpError(status, QString::fromUtf8(raw), QString()));
            return;
        }
        if (status < 200 || status >= 300) {
            emit downloadFailed(mapHttpError(
                status, QString::fromUtf8(raw),
                QStringLiteral("Téléchargement OpenSubtitles échoué.")));
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(raw);
        if (!doc.isObject()) {
            emit downloadFailed(QStringLiteral("Réponse download OpenSubtitles invalide"));
            return;
        }

        const QJsonObject obj = doc.object();
        const QString link = obj.value(QStringLiteral("link")).toString().trimmed();
        if (link.isEmpty()) {
            emit downloadFailed(QStringLiteral("Lien temporaire OpenSubtitles manquant"));
            return;
        }

        QVariantMap meta;
        meta.insert(QStringLiteral("fileId"), fileId);
        meta.insert(QStringLiteral("fileName"), obj.value(QStringLiteral("file_name")).toString());
        meta.insert(QStringLiteral("remaining"), obj.value(QStringLiteral("remaining")).toVariant());
        meta.insert(QStringLiteral("requests"), obj.value(QStringLiteral("requests")).toVariant());
        meta.insert(QStringLiteral("message"), obj.value(QStringLiteral("message")).toString());
        meta.insert(QStringLiteral("resetTime"),
                    obj.value(QStringLiteral("reset_time")).toString());
        meta.insert(QStringLiteral("resetTimeUtc"),
                    obj.value(QStringLiteral("reset_time_utc")).toString());
        meta.insert(QStringLiteral("link"), link);
        meta.insert(QStringLiteral("source"), QStringLiteral("com"));

        fetchSubtitleFile(fileId, QUrl(link), meta);
    });
}

void OpenSubtitlesCom::login(const QString& username, const QString& password)
{
    if (m_apiKey.isEmpty()) {
        emit loginFailed(QStringLiteral("Clé API OpenSubtitles manquante"));
        return;
    }
    const QString user = username.trimmed();
    if (user.isEmpty() || password.isEmpty()) {
        emit loginFailed(QStringLiteral("Identifiants OpenSubtitles incomplets"));
        return;
    }

    const QUrl url(QString::fromLatin1(kApiBase) + QStringLiteral("/login"));
    QJsonObject body;
    body.insert(QStringLiteral("username"), user);
    body.insert(QStringLiteral("password"), password);
    const QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);

    QNetworkReply* reply = m_nam.post(makeApiRequest(url), payload);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray raw = reply->readAll();

        if (reply->error() != QNetworkReply::NoError
            || status < 200 || status >= 300) {
            emit loginFailed(mapHttpError(status, QString::fromUtf8(raw),
                                          QStringLiteral("Login OpenSubtitles échoué.")));
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(raw);
        if (!doc.isObject()) {
            emit loginFailed(QStringLiteral("Réponse login OpenSubtitles invalide"));
            return;
        }

        const QJsonObject obj = doc.object();
        const QString token = obj.value(QStringLiteral("token")).toString().trimmed();
        if (token.isEmpty()) {
            emit loginFailed(QStringLiteral("Token JWT OpenSubtitles manquant"));
            return;
        }

        m_jwt = token;
        QVariantMap info;
        info.insert(QStringLiteral("token"), token);
        info.insert(QStringLiteral("user"), obj.value(QStringLiteral("user")).toVariant());
        info.insert(QStringLiteral("status"), obj.value(QStringLiteral("status")).toVariant());
        emit loginFinished(info);
    });
}

void OpenSubtitlesCom::fetchSubtitleFile(qint64 fileId, const QUrl& link, QVariantMap meta)
{
    if (!link.isValid()) {
        emit downloadFailed(QStringLiteral("URL temporaire OpenSubtitles invalide"));
        return;
    }

    QNetworkRequest req{link};
    req.setRawHeader("User-Agent", userAgent().toUtf8());
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, fileId, meta]() mutable {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit downloadFailed(QStringLiteral("Échec du téléchargement du fichier SRT : %1")
                                    .arg(reply->errorString()));
            return;
        }

        const QByteArray body = reply->readAll();
        if (body.isEmpty()) {
            emit downloadFailed(QStringLiteral("Fichier sous-titres vide"));
            return;
        }

        const QString base =
            QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        const QString dirPath = base + QStringLiteral("/subtitles");
        if (!QDir().mkpath(dirPath)) {
            emit downloadFailed(QStringLiteral("Impossible de créer le dossier cache sous-titres"));
            return;
        }

        QString fileName = meta.value(QStringLiteral("fileName")).toString().trimmed();
        if (fileName.isEmpty())
            fileName = QStringLiteral("%1.srt").arg(fileId);
        fileName.replace(QLatin1Char('/'), QLatin1Char('_'));
        fileName.replace(QLatin1Char('\\'), QLatin1Char('_'));

        const qint64 featureId = meta.value(QStringLiteral("featureId")).toLongLong();
        const QString localName =
            QStringLiteral("%1_%2_%3")
                .arg(featureId)
                .arg(fileId)
                .arg(fileName);
        const QString localPath = dirPath + QLatin1Char('/') + localName;

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
}

} // namespace app
