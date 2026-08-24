#include "mcp_tools.h"

#include "appcontroller.h"
#include "filmassistant.h"
#include "mcp_json.h"

#include "core/phraseextract.h"
#include "core/srtcodec.h"
#include "net/opensubtitlesclient.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QTimer>

namespace app::mcp {
namespace {

constexpr int kOsTimeoutMs = 30000;

// Session MCP film : dernier SRT téléchargé / prévisualisé (indices → import).
QString g_lastSubtitlePath;
std::vector<core::Cue> g_lastCues;

QString formatCueTime(int ms)
{
    if (ms < 0)
        ms = 0;
    const int totalSec = ms / 1000;
    const int h = totalSec / 3600;
    const int m = (totalSec % 3600) / 60;
    const int s = totalSec % 60;
    const int frac = ms % 1000;
    return QStringLiteral("%1:%2:%3.%4")
        .arg(h, 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'))
        .arg(frac, 3, 10, QChar('0'));
}

QJsonObject cueToJson(const core::Cue& c, int index)
{
    QJsonObject o;
    o.insert(QStringLiteral("index"), index);
    o.insert(QStringLiteral("startMs"), c.startMs);
    o.insert(QStringLiteral("endMs"), c.endMs);
    o.insert(QStringLiteral("start"), formatCueTime(c.startMs));
    o.insert(QStringLiteral("end"), formatCueTime(c.endMs));
    o.insert(QStringLiteral("text"), QString::fromStdString(c.text));
    o.insert(QStringLiteral("plain"), QString::fromStdString(c.plain.empty() ? c.text : c.plain));
    o.insert(QStringLiteral("likelySfx"), c.likelySfx);
    return o;
}

bool loadCuesFromPath(const QString& path, std::vector<core::Cue>* out, QString* error)
{
    if (path.trimmed().isEmpty()) {
        *error = QStringLiteral("path required (or download_subtitle first)");
        return false;
    }
    const auto parsed = core::parseSrtFile(path.toStdString());
    if (!parsed.errors.empty() && parsed.cues.empty()) {
        *error = QString::fromStdString(parsed.errors.front());
        return false;
    }
    *out = parsed.cues;
    g_lastSubtitlePath = path;
    g_lastCues = parsed.cues;
    return true;
}

} // namespace

QStringList labelsFromCachedCueIndices(const QJsonArray& indices, QString* error)
{
    if (g_lastCues.empty()) {
        *error = QStringLiteral("No cues cached — call preview_cues first");
        return {};
    }
    QStringList labels;
    for (const QJsonValue& v : indices) {
        const int idx = v.toInt(-1);
        if (idx < 0 || idx >= static_cast<int>(g_lastCues.size()))
            continue;
        const core::Cue& c = g_lastCues[static_cast<size_t>(idx)];
        const QString t = QString::fromStdString(c.plain.empty() ? c.text : c.plain).trimmed();
        if (!t.isEmpty())
            labels.append(t);
    }
    return labels;
}

QJsonObject invokeFilmTool(AppController* ctrl, const QString& name, const QJsonObject& args)
{
    FilmAssistant* film = ctrl ? ctrl->film() : nullptr;

    if (name == QLatin1String("search_subtitles")) {
        const QString query = args.value(QStringLiteral("query")).toString().trimmed();
        if (query.isEmpty())
            return toolError(QStringLiteral("query required"));
        QString languages = args.value(QStringLiteral("languages")).toString().trimmed();
        if (languages.isEmpty() && film && !film->language().isEmpty())
            languages = film->language();
        QString hearing = args.value(QStringLiteral("hearing_impaired")).toString().trimmed();
        if (hearing.isEmpty())
            hearing = QStringLiteral("include");
        Q_UNUSED(args.value(QStringLiteral("type"))); // reserved; client uses movie by default

        OpenSubtitlesClient client;
        if (film)
            client.setApiKey(film->apiKey());

        QEventLoop loop;
        bool done = false;
        QString err;
        QVariantList results;
        QObject::connect(&client, &OpenSubtitlesClient::searchFinished, &loop,
                         [&](const QVariantList& r) {
                             results = r;
                             done = true;
                             loop.quit();
                         });
        QObject::connect(&client, &OpenSubtitlesClient::searchFailed, &loop,
                         [&](const QString& m) {
                             err = m;
                             done = true;
                             loop.quit();
                         });
        client.search(query, languages, hearing);
        if (!done) {
            QTimer::singleShot(kOsTimeoutMs, &loop, &QEventLoop::quit);
            loop.exec();
        }
        if (!done)
            return toolError(QStringLiteral("search_subtitles timed out after %1 ms").arg(kOsTimeoutMs));
        if (!err.isEmpty())
            return toolError(err);

        QJsonArray list;
        for (const QVariant& v : results)
            list.append(QJsonObject::fromVariantMap(v.toMap()));
        return toolResultObj({{QStringLiteral("query"), query},
                              {QStringLiteral("languages"), languages},
                              {QStringLiteral("hearing_impaired"), hearing},
                              {QStringLiteral("count"), list.size()},
                              {QStringLiteral("results"), list}});
    }

    if (name == QLatin1String("download_subtitle")) {
        const qint64 fileId = static_cast<qint64>(args.value(QStringLiteral("file_id")).toDouble());
        if (fileId <= 0)
            return toolError(QStringLiteral("file_id required (positive)"));

        OpenSubtitlesClient client;
        if (film)
            client.setApiKey(film->apiKey());

        QEventLoop loop;
        bool done = false;
        QString err;
        QString localPath;
        QVariantMap meta;
        QObject::connect(&client, &OpenSubtitlesClient::downloadFinished, &loop,
                         [&](const QString& path, const QVariantMap& m) {
                             localPath = path;
                             meta = m;
                             done = true;
                             loop.quit();
                         });
        QObject::connect(&client, &OpenSubtitlesClient::downloadFailed, &loop,
                         [&](const QString& m) {
                             err = m;
                             done = true;
                             loop.quit();
                         });
        client.download(fileId);
        if (!done) {
            QTimer::singleShot(kOsTimeoutMs, &loop, &QEventLoop::quit);
            loop.exec();
        }
        if (!done)
            return toolError(QStringLiteral("download_subtitle timed out after %1 ms").arg(kOsTimeoutMs));
        if (!err.isEmpty())
            return toolError(err);

        g_lastSubtitlePath = localPath;
        g_lastCues.clear();
        QJsonObject out{{QStringLiteral("file_id"), fileId},
                        {QStringLiteral("path"), localPath},
                        {QStringLiteral("meta"), QJsonObject::fromVariantMap(meta)}};
        return toolResultObj(out);
    }

    if (name == QLatin1String("preview_cues")) {
        QString path = args.value(QStringLiteral("path")).toString().trimmed();
        if (path.isEmpty())
            path = g_lastSubtitlePath;
        std::vector<core::Cue> cues;
        QString err;
        if (!loadCuesFromPath(path, &cues, &err))
            return toolError(err);

        const bool skipSfx = args.value(QStringLiteral("skip_sfx")).toBool(false);
        int maxN = args.contains(QStringLiteral("max")) ? args.value(QStringLiteral("max")).toInt() : 50;
        if (maxN <= 0)
            maxN = 50;

        QJsonArray list;
        int total = 0;
        int skippedSfx = 0;
        for (int i = 0; i < static_cast<int>(cues.size()); ++i) {
            const core::Cue& c = cues[static_cast<size_t>(i)];
            ++total;
            if (skipSfx && c.likelySfx) {
                ++skippedSfx;
                continue;
            }
            if (list.size() >= maxN)
                continue;
            list.append(cueToJson(c, i));
        }
        return toolResultObj({{QStringLiteral("path"), path},
                              {QStringLiteral("total"), total},
                              {QStringLiteral("skippedSfx"), skippedSfx},
                              {QStringLiteral("returned"), list.size()},
                              {QStringLiteral("truncated"), list.size() < total - skippedSfx
                                  || (skipSfx && skippedSfx > 0 && list.size() >= maxN)},
                              {QStringLiteral("cues"), list}});
    }

    if (name == QLatin1String("suggest_bingo_phrases")) {
        QString path = args.value(QStringLiteral("path")).toString().trimmed();
        if (path.isEmpty())
            path = g_lastSubtitlePath;
        std::vector<core::Cue> cues;
        QString err;
        if (!loadCuesFromPath(path, &cues, &err))
            return toolError(err);

        core::PhraseSuggestOptions opt;
        if (args.contains(QStringLiteral("skip_sfx")))
            opt.skipSfx = args.value(QStringLiteral("skip_sfx")).toBool();
        if (args.contains(QStringLiteral("max_len")))
            opt.maxLen = args.value(QStringLiteral("max_len")).toInt();
        if (args.contains(QStringLiteral("max_phrases")))
            opt.maxPhrases = args.value(QStringLiteral("max_phrases")).toInt();
        if (args.contains(QStringLiteral("dedupe")))
            opt.dedupe = args.value(QStringLiteral("dedupe")).toBool();

        const auto phrases = core::suggestBingoPhrases(cues, opt);
        QJsonArray labels;
        for (const std::string& p : phrases)
            labels.append(QString::fromStdString(p));
        return toolResultObj({{QStringLiteral("path"), path},
                              {QStringLiteral("count"), labels.size()},
                              {QStringLiteral("phrases"), labels},
                              {QStringLiteral("options"),
                               QJsonObject{{QStringLiteral("skip_sfx"), opt.skipSfx},
                                           {QStringLiteral("max_len"), opt.maxLen},
                                           {QStringLiteral("max_phrases"), opt.maxPhrases},
                                           {QStringLiteral("dedupe"), opt.dedupe}}}});
    }

    return {};
}

} // namespace app::mcp
