// Intégration live OpenSubtitles — skippable sans clé.
// ctest -L opensubtitles
// Env: OPENSUBTITLES_API_KEY (jamais committer).
// Ne consomme pas de quota download : search GET uniquement.

#include <QtTest>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

class OpenSubtitlesLiveTest : public QObject
{
    Q_OBJECT
private slots:
    void searchHearingImpairedInclude()
    {
        const QByteArray key = qgetenv("OPENSUBTITLES_API_KEY").trimmed();
        if (key.isEmpty())
            QSKIP("OPENSUBTITLES_API_KEY unset — live OpenSubtitles skipped");

        QNetworkAccessManager nam;
        QUrl url(QStringLiteral("https://api.opensubtitles.com/api/v1/subtitles"));
        QUrlQuery q;
        q.addQueryItem(QStringLiteral("query"), QStringLiteral("Matrix"));
        q.addQueryItem(QStringLiteral("languages"), QStringLiteral("fr"));
        q.addQueryItem(QStringLiteral("type"), QStringLiteral("movie"));
        // Décision figée #3 : hearing_impaired=include
        q.addQueryItem(QStringLiteral("hearing_impaired"), QStringLiteral("include"));
        url.setQuery(q);

        QNetworkRequest req(url);
        req.setRawHeader("Api-Key", key);
        req.setRawHeader("User-Agent", "OpenBingo v2.0.28");
        req.setRawHeader("Accept", "application/json");
        req.setTransferTimeout(20000);

        QNetworkReply* reply = nam.get(req);
        QEventLoop loop;
        QTimer watchdog;
        watchdog.setSingleShot(true);
        connect(&watchdog, &QTimer::timeout, &loop, &QEventLoop::quit);
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        watchdog.start(25000);
        loop.exec();

        QVERIFY2(reply->isFinished(), "timeout OpenSubtitles /subtitles");
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        reply->deleteLater();

        QVERIFY2(status == 200,
                 qPrintable(QStringLiteral("HTTP %1 body=%2")
                                .arg(status)
                                .arg(QString::fromUtf8(body.left(400)))));

        const QJsonDocument doc = QJsonDocument::fromJson(body);
        QVERIFY(doc.isObject());
        const QJsonArray data = doc.object().value(QStringLiteral("data")).toArray();
        QVERIFY2(!data.isEmpty(), "aucun résultat Matrix FR (API OK mais vide)");

        // Contrat résultat : au moins un attribut hearing_impaired présent
        bool sawHiField = false;
        for (const QJsonValue& v : data) {
            const QJsonObject attrs = v.toObject()
                                          .value(QStringLiteral("attributes"))
                                          .toObject();
            if (attrs.contains(QStringLiteral("hearing_impaired"))) {
                sawHiField = true;
                break;
            }
        }
        QVERIFY2(sawHiField, "champ hearing_impaired absent des attributs");
    }
};

QTEST_MAIN(OpenSubtitlesLiveTest)
#include "tst_opensubtitles_live.moc"
