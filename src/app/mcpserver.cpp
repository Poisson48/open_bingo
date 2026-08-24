#include "mcpserver.h"
#include "appcontroller.h"
#include "filmassistant.h"
#include "projectmodel.h"

#include "core/phraseextract.h"
#include "core/srtcodec.h"
#include "net/opensubtitlesclient.h"

#include <QClipboard>
#include <QEventLoop>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QModelIndex>
#include <QTimer>
#include <QUuid>

#ifndef Q_OS_ANDROID
#  include <QHostAddress>
#  include <QTcpServer>
#  include <QTcpSocket>
#endif

namespace app {
namespace {

#ifndef Q_OS_ANDROID

constexpr int kPreviewLimit = 30;
constexpr int kAddCasesSoftWarn = 500;
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

QByteArray httpResponse(int status, const char* reason, const QByteArray& body,
                        const char* contentType = "application/json")
{
    QByteArray out;
    out += "HTTP/1.1 ";
    out += QByteArray::number(status);
    out += ' ';
    out += reason;
    out += "\r\n";
    out += "Content-Type: ";
    out += contentType;
    out += "\r\n";
    out += "Content-Length: ";
    out += QByteArray::number(body.size());
    out += "\r\n";
    out += "Connection: close\r\n";
    out += "Access-Control-Allow-Origin: *\r\n";
    out += "Access-Control-Allow-Headers: Authorization, Content-Type, Accept, Mcp-Session-Id\r\n";
    out += "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n";
    if (status == 200 && !body.isEmpty())
        out += "Cache-Control: no-store\r\n";
    out += "\r\n";
    out += body;
    return out;
}

QString headerValue(const QList<QPair<QByteArray, QByteArray>>& headers, const QByteArray& name)
{
    for (const auto& h : headers) {
        if (h.first.compare(name, Qt::CaseInsensitive) == 0)
            return QString::fromUtf8(h.second.trimmed());
    }
    return {};
}

bool parseHttpRequest(const QByteArray& raw, QByteArray* method, QByteArray* path,
                      QList<QPair<QByteArray, QByteArray>>* headers, QByteArray* body)
{
    const int sep = raw.indexOf("\r\n\r\n");
    if (sep < 0)
        return false;
    const QByteArray head = raw.left(sep);
    const QList<QByteArray> lines = head.split('\n');
    if (lines.isEmpty())
        return false;
    const QByteArray reqLine = lines.first().trimmed();
    const QList<QByteArray> parts = reqLine.split(' ');
    if (parts.size() < 2)
        return false;
    *method = parts[0];
    *path = parts[1];
    headers->clear();
    for (int i = 1; i < lines.size(); ++i) {
        QByteArray line = lines[i];
        if (line.endsWith('\r'))
            line.chop(1);
        const int colon = line.indexOf(':');
        if (colon <= 0)
            continue;
        headers->append({line.left(colon).trimmed(), line.mid(colon + 1).trimmed()});
    }
    int contentLength = 0;
    const QString cl = headerValue(*headers, "Content-Length");
    if (!cl.isEmpty())
        contentLength = cl.toInt();
    if (contentLength < 0)
        return false;
    if (raw.size() < sep + 4 + contentLength)
        return false;
    *body = raw.mid(sep + 4, contentLength);
    return true;
}

QJsonObject rpcError(const QJsonValue& id, int code, const QString& message)
{
    QJsonObject err;
    err.insert(QStringLiteral("code"), code);
    err.insert(QStringLiteral("message"), message);
    QJsonObject out;
    out.insert(QStringLiteral("jsonrpc"), QStringLiteral("2.0"));
    out.insert(QStringLiteral("id"), id);
    out.insert(QStringLiteral("error"), err);
    return out;
}

QJsonObject rpcResult(const QJsonValue& id, const QJsonValue& result)
{
    QJsonObject out;
    out.insert(QStringLiteral("jsonrpc"), QStringLiteral("2.0"));
    out.insert(QStringLiteral("id"), id);
    out.insert(QStringLiteral("result"), result);
    return out;
}

QJsonObject toolResultObj(const QJsonObject& obj, bool isError = false)
{
    QJsonObject contentItem;
    contentItem.insert(QStringLiteral("type"), QStringLiteral("text"));
    contentItem.insert(QStringLiteral("text"),
                       QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
    QJsonObject out;
    out.insert(QStringLiteral("content"), QJsonArray{contentItem});
    if (isError)
        out.insert(QStringLiteral("isError"), true);
    return out;
}

QJsonObject toolError(const QString& message)
{
    return toolResultObj(QJsonObject{{QStringLiteral("error"), message}}, true);
}

QJsonObject toolDef(const QString& name, const QString& description, const QJsonObject& inputSchema)
{
    QJsonObject t;
    t.insert(QStringLiteral("name"), name);
    t.insert(QStringLiteral("description"), description);
    t.insert(QStringLiteral("inputSchema"), inputSchema);
    return t;
}

QJsonObject objectSchema(const QJsonObject& properties, const QStringList& required = {})
{
    QJsonObject s;
    s.insert(QStringLiteral("type"), QStringLiteral("object"));
    s.insert(QStringLiteral("properties"), properties);
    if (!required.isEmpty()) {
        QJsonArray req;
        for (const QString& r : required)
            req.append(r);
        s.insert(QStringLiteral("required"), req);
    }
    return s;
}

QJsonArray toolsCatalog()
{
    const auto str = [](const QString& desc = {}) {
        QJsonObject o{{QStringLiteral("type"), QStringLiteral("string")}};
        if (!desc.isEmpty())
            o.insert(QStringLiteral("description"), desc);
        return o;
    };
    const auto num = [](const QString& desc = {}) {
        QJsonObject o{{QStringLiteral("type"), QStringLiteral("number")}};
        if (!desc.isEmpty())
            o.insert(QStringLiteral("description"), desc);
        return o;
    };
    const auto boolean = [](const QString& desc = {}) {
        QJsonObject o{{QStringLiteral("type"), QStringLiteral("boolean")}};
        if (!desc.isEmpty())
            o.insert(QStringLiteral("description"), desc);
        return o;
    };

    QJsonArray tools;
    tools.append(toolDef(QStringLiteral("list_projects"),
                         QStringLiteral("Liste les projets (id, title, updatedAt, counts)."),
                         objectSchema({})));
    tools.append(toolDef(QStringLiteral("get_project"),
                         QStringLiteral("Résumé config + aperçu phrases/gages du projet courant (ou id)."),
                         objectSchema({{QStringLiteral("id"), str(QStringLiteral("Project id (optional)"))}})));
    tools.append(toolDef(QStringLiteral("create_project"),
                         QStringLiteral("Crée un projet et l'ouvre."),
                         objectSchema({{QStringLiteral("title"), str()},
                                       {QStringLiteral("description"), str()}})));
    tools.append(toolDef(QStringLiteral("open_project"),
                         QStringLiteral("Ouvre un projet (courant pour la session MCP/UI)."),
                         objectSchema({{QStringLiteral("id"), str()}}, {QStringLiteral("id")})));
    tools.append(toolDef(
        QStringLiteral("set_config"),
        QStringLiteral("Met à jour la config du projet ouvert (sans wipe forcé des grilles)."),
        objectSchema({{QStringLiteral("gridRows"), num()},
                      {QStringLiteral("gridCols"), num()},
                      {QStringLiteral("gageMode"), boolean()},
                      {QStringLiteral("freeCenter"), boolean()},
                      {QStringLiteral("startHP"), num()},
                      {QStringLiteral("title"), str()},
                      {QStringLiteral("description"), str()},
                      {QStringLiteral("players"),
                       QJsonObject{{QStringLiteral("type"), QStringLiteral("array")},
                                   {QStringLiteral("items"),
                                    QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}}}}})));
    tools.append(toolDef(
        QStringLiteral("add_cases"),
        QStringLiteral("Ajoute des phrases en bulk."),
        objectSchema(
            {{QStringLiteral("cases"),
              QJsonObject{
                  {QStringLiteral("type"), QStringLiteral("array")},
                  {QStringLiteral("items"),
                   objectSchema({{QStringLiteral("label"), str()},
                                 {QStringLiteral("points"), num()},
                                 {QStringLiteral("rate"), num()}},
                                {QStringLiteral("label")})}}}},
            {QStringLiteral("cases")})));
    tools.append(toolDef(
        QStringLiteral("add_gages"),
        QStringLiteral("Ajoute des gages en bulk."),
        objectSchema(
            {{QStringLiteral("gages"),
              QJsonObject{
                  {QStringLiteral("type"), QStringLiteral("array")},
                  {QStringLiteral("items"),
                   objectSchema({{QStringLiteral("description"), str()},
                                 {QStringLiteral("number"), num()},
                                 {QStringLiteral("hp"), num()},
                                 {QStringLiteral("rate"), num()}},
                                {QStringLiteral("description")})}}}},
            {QStringLiteral("gages")})));
    tools.append(toolDef(QStringLiteral("import_phrases_csv"),
                         QStringLiteral("Importe phrases.csv (texte ou chemin local)."),
                         objectSchema({{QStringLiteral("csv"), str()},
                                       {QStringLiteral("path"), str()},
                                       {QStringLiteral("replace"), boolean()}})));
    tools.append(toolDef(QStringLiteral("import_gages_csv"),
                         QStringLiteral("Importe gages.csv (texte ou chemin local)."),
                         objectSchema({{QStringLiteral("csv"), str()},
                                       {QStringLiteral("path"), str()},
                                       {QStringLiteral("replace"), boolean()}})));
    tools.append(toolDef(QStringLiteral("export_phrases_csv"),
                         QStringLiteral("Exporte les phrases du projet courant en CSV."),
                         objectSchema({})));
    tools.append(toolDef(QStringLiteral("export_gages_csv"),
                         QStringLiteral("Exporte les gages du projet courant en CSV."),
                         objectSchema({})));
    tools.append(toolDef(QStringLiteral("clear_cases"),
                         QStringLiteral("Vide toutes les phrases (confirm=true requis)."),
                         objectSchema({{QStringLiteral("confirm"), boolean()}},
                                      {QStringLiteral("confirm")})));
    tools.append(toolDef(QStringLiteral("clear_gages"),
                         QStringLiteral("Vide tous les gages (confirm=true requis)."),
                         objectSchema({{QStringLiteral("confirm"), boolean()}},
                                      {QStringLiteral("confirm")})));
    tools.append(toolDef(QStringLiteral("generate_grids"),
                         QStringLiteral("Génère les grilles du projet courant."),
                         objectSchema({})));
    tools.append(toolDef(QStringLiteral("project_stats"),
                         QStringLiteral("Stats: minCases, availableCells, gridsDirty, counts."),
                         objectSchema({})));

    // Film / OpenSubtitles (PLAN-opensubtitles) — search/download sync via QEventLoop 30s.
    tools.append(toolDef(
        QStringLiteral("search_subtitles"),
        QStringLiteral("Cherche des sous-titres OpenSubtitles (Api-Key app requise)."),
        objectSchema(
            {{QStringLiteral("query"), str(QStringLiteral("Titre du film"))},
             {QStringLiteral("languages"),
              str(QStringLiteral("Codes langue CSV, ex. fr,en (optionnel)"))},
             {QStringLiteral("hearing_impaired"),
              str(QStringLiteral("include|only|exclude — défaut include"))},
             {QStringLiteral("type"), str(QStringLiteral("movie|series|… (info, défaut movie)"))}},
            {QStringLiteral("query")})));
    tools.append(toolDef(
        QStringLiteral("download_subtitle"),
        QStringLiteral("Télécharge un sous-titre par file_id → chemin local cache."),
        objectSchema({{QStringLiteral("file_id"), num(QStringLiteral("OpenSubtitles file_id"))}},
                     {QStringLiteral("file_id")})));
    tools.append(toolDef(
        QStringLiteral("preview_cues"),
        QStringLiteral("Parse un .srt local (ou dernier download) → aperçu cues."),
        objectSchema({{QStringLiteral("path"), str(QStringLiteral("Chemin .srt (sinon dernier DL)"))},
                      {QStringLiteral("max"), num(QStringLiteral("Nombre max de cues renvoyés"))},
                      {QStringLiteral("skip_sfx"),
                       boolean(QStringLiteral("Omettre les cues likelySfx"))}})));
    tools.append(toolDef(
        QStringLiteral("suggest_bingo_phrases"),
        QStringLiteral("Heuristique locale → labels bingo candidats depuis un SRT."),
        objectSchema({{QStringLiteral("path"), str(QStringLiteral("Chemin .srt (sinon dernier)"))},
                      {QStringLiteral("skip_sfx"), boolean()},
                      {QStringLiteral("max_len"), num()},
                      {QStringLiteral("max_phrases"), num()},
                      {QStringLiteral("dedupe"), boolean()}})));
    tools.append(toolDef(
        QStringLiteral("import_cues_as_cases"),
        QStringLiteral("Ajoute des phrases au projet ouvert (texts[] ou indices[] sur last preview)."),
        objectSchema({{QStringLiteral("texts"),
                       QJsonObject{{QStringLiteral("type"), QStringLiteral("array")},
                                   {QStringLiteral("items"),
                                    QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}}}},
                      {QStringLiteral("indices"),
                       QJsonObject{{QStringLiteral("type"), QStringLiteral("array")},
                                   {QStringLiteral("items"),
                                    QJsonObject{{QStringLiteral("type"), QStringLiteral("number")}}}}}})));
    return tools;
}

QString readCsvArg(const QJsonObject& args, QString* error)
{
    if (args.contains(QStringLiteral("csv")))
        return args.value(QStringLiteral("csv")).toString();
    if (args.contains(QStringLiteral("path"))) {
        const QString path = args.value(QStringLiteral("path")).toString();
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            *error = QStringLiteral("Cannot read file: %1").arg(path);
            return {};
        }
        return QString::fromUtf8(f.readAll());
    }
    *error = QStringLiteral("Provide csv text or path");
    return {};
}

QJsonObject variantMapToJson(const QVariantMap& m)
{
    return QJsonObject::fromVariantMap(m);
}

QJsonObject invokeFilmTool(AppController* ctrl, const QString& name, const QJsonObject& args)
{
    FilmAssistant* film = ctrl->film();

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

QJsonObject invokeTool(AppController* ctrl, const QString& name, const QJsonObject& args)
{
    if (!ctrl)
        return toolError(QStringLiteral("No controller"));

    // Film tools (no project required except import_cues_as_cases).
    if (name == QLatin1String("search_subtitles") || name == QLatin1String("download_subtitle")
        || name == QLatin1String("preview_cues") || name == QLatin1String("suggest_bingo_phrases")) {
        return invokeFilmTool(ctrl, name, args);
    }

    auto* model = ctrl->projects();

    if (name == QLatin1String("list_projects")) {
        QJsonArray list;
        if (model) {
            const QString currentId = ctrl->currentProjectId();
            const int n = model->rowCount();
            for (int i = 0; i < n; ++i) {
                const QModelIndex idx = model->index(i, 0);
                QJsonObject row;
                row.insert(QStringLiteral("id"),
                           model->data(idx, ProjectModel::IdRole).toString());
                row.insert(QStringLiteral("title"),
                           model->data(idx, ProjectModel::TitleRole).toString());
                row.insert(QStringLiteral("updatedAt"),
                           model->data(idx, ProjectModel::UpdatedAtRole).toLongLong());
                row.insert(QStringLiteral("cases"),
                           model->data(idx, ProjectModel::CaseCountRole).toInt());
                row.insert(QStringLiteral("players"),
                           model->data(idx, ProjectModel::PlayerCountRole).toInt());
                const QString id = row.value(QStringLiteral("id")).toString();
                if (id == currentId)
                    row.insert(QStringLiteral("gages"), ctrl->gages().size());
                list.append(row);
            }
        }
        return toolResultObj({{QStringLiteral("projects"), list}});
    }

    if (name == QLatin1String("create_project")) {
        const QString id = ctrl->createProject();
        if (id.isEmpty())
            return toolError(QStringLiteral("createProject failed"));
        const QString title = args.value(QStringLiteral("title")).toString().trimmed();
        const QString description = args.value(QStringLiteral("description")).toString();
        if (!title.isEmpty() || args.contains(QStringLiteral("description")))
            ctrl->updateProjectMeta(id, title.isEmpty() ? ctrl->title() : title, description);
        return toolResultObj({{QStringLiteral("id"), id},
                              {QStringLiteral("title"), ctrl->title()},
                              {QStringLiteral("description"), ctrl->description()}});
    }

    if (name == QLatin1String("open_project")) {
        const QString id = args.value(QStringLiteral("id")).toString().trimmed();
        if (id.isEmpty())
            return toolError(QStringLiteral("id required"));
        if (!ctrl->openProject(id, false))
            return toolError(QStringLiteral("Project not found"));
        return toolResultObj({{QStringLiteral("ok"), true},
                              {QStringLiteral("id"), id},
                              {QStringLiteral("title"), ctrl->title()}});
    }

    // Remaining tools need a current project (open optional id for get_project).
    if (name == QLatin1String("get_project")) {
        const QString id = args.value(QStringLiteral("id")).toString().trimmed();
        if (!id.isEmpty() && id != ctrl->currentProjectId()) {
            if (!ctrl->openProject(id, false))
                return toolError(QStringLiteral("Project not found"));
        }
        if (ctrl->currentProjectId().isEmpty())
            return toolError(QStringLiteral("No project open"));

        QJsonObject cfg;
        cfg.insert(QStringLiteral("gridRows"), ctrl->gridRows());
        cfg.insert(QStringLiteral("gridCols"), ctrl->gridCols());
        cfg.insert(QStringLiteral("gageMode"), ctrl->gageMode());
        cfg.insert(QStringLiteral("freeCenter"), ctrl->freeCenter());
        cfg.insert(QStringLiteral("startHP"), ctrl->startHP());
        cfg.insert(QStringLiteral("gridsDirty"), ctrl->gridsDirty());

        QJsonArray casesPrev;
        const QVariantList cases = ctrl->cases();
        for (int i = 0; i < cases.size() && i < kPreviewLimit; ++i)
            casesPrev.append(QJsonObject::fromVariantMap(cases[i].toMap()));

        QJsonArray gagesPrev;
        const QVariantList gages = ctrl->gages();
        for (int i = 0; i < gages.size() && i < kPreviewLimit; ++i)
            gagesPrev.append(QJsonObject::fromVariantMap(gages[i].toMap()));

        QJsonObject out;
        out.insert(QStringLiteral("id"), ctrl->currentProjectId());
        out.insert(QStringLiteral("title"), ctrl->title());
        out.insert(QStringLiteral("description"), ctrl->description());
        out.insert(QStringLiteral("config"), cfg);
        out.insert(QStringLiteral("casesCount"), cases.size());
        out.insert(QStringLiteral("gagesCount"), gages.size());
        out.insert(QStringLiteral("playersCount"), ctrl->players().size());
        out.insert(QStringLiteral("gridsCount"), ctrl->grids().size());
        out.insert(QStringLiteral("casesPreview"), casesPrev);
        out.insert(QStringLiteral("gagesPreview"), gagesPrev);
        out.insert(QStringLiteral("previewTruncated"),
                   cases.size() > kPreviewLimit || gages.size() > kPreviewLimit);
        return toolResultObj(out);
    }

    if (ctrl->currentProjectId().isEmpty())
        return toolError(QStringLiteral("No project open — call open_project or create_project first"));

    if (name == QLatin1String("set_config")) {
        if (args.contains(QStringLiteral("title")))
            ctrl->setTitle(args.value(QStringLiteral("title")).toString());
        if (args.contains(QStringLiteral("description")))
            ctrl->setDescription(args.value(QStringLiteral("description")).toString());
        if (args.contains(QStringLiteral("gridRows")))
            ctrl->setGridRows(args.value(QStringLiteral("gridRows")).toInt());
        if (args.contains(QStringLiteral("gridCols")))
            ctrl->setGridCols(args.value(QStringLiteral("gridCols")).toInt());
        if (args.contains(QStringLiteral("gageMode")))
            ctrl->setGageMode(args.value(QStringLiteral("gageMode")).toBool());
        if (args.contains(QStringLiteral("freeCenter")))
            ctrl->setFreeCenter(args.value(QStringLiteral("freeCenter")).toBool());
        if (args.contains(QStringLiteral("startHP")))
            ctrl->setStartHP(args.value(QStringLiteral("startHP")).toInt());
        if (args.contains(QStringLiteral("players")) && args.value(QStringLiteral("players")).isArray()) {
            const QJsonArray wanted = args.value(QStringLiteral("players")).toArray();
            while (ctrl->players().size() > wanted.size())
                ctrl->removePlayer(ctrl->players().size() - 1);
            while (ctrl->players().size() < wanted.size())
                ctrl->addPlayer();
            for (int i = 0; i < wanted.size(); ++i) {
                const QString pname = wanted[i].toString().trimmed();
                if (!pname.isEmpty())
                    ctrl->setPlayerName(i, pname);
            }
        }
        return toolResultObj({{QStringLiteral("ok"), true},
                              {QStringLiteral("gridRows"), ctrl->gridRows()},
                              {QStringLiteral("gridCols"), ctrl->gridCols()},
                              {QStringLiteral("gageMode"), ctrl->gageMode()},
                              {QStringLiteral("freeCenter"), ctrl->freeCenter()},
                              {QStringLiteral("startHP"), ctrl->startHP()},
                              {QStringLiteral("gridsDirty"), ctrl->gridsDirty()},
                              {QStringLiteral("players"), ctrl->players().size()}});
    }

    if (name == QLatin1String("add_cases")) {
        const QJsonArray cases = args.value(QStringLiteral("cases")).toArray();
        if (cases.isEmpty())
            return toolError(QStringLiteral("cases array required"));
        int added = 0;
        for (const QJsonValue& v : cases) {
            const QJsonObject c = v.toObject();
            const QString label = c.value(QStringLiteral("label")).toString().trimmed();
            if (label.isEmpty())
                continue;
            const int points = c.contains(QStringLiteral("points")) ? c.value(QStringLiteral("points")).toInt() : 1;
            const int rate = c.contains(QStringLiteral("rate")) ? c.value(QStringLiteral("rate")).toInt(50) : 50;
            ctrl->addCase(label, points, rate);
            ++added;
        }
        QJsonObject out{{QStringLiteral("added"), added},
                        {QStringLiteral("casesTotal"), ctrl->cases().size()},
                        {QStringLiteral("gridsDirty"), ctrl->gridsDirty()}};
        if (added > kAddCasesSoftWarn)
            out.insert(QStringLiteral("warning"),
                       QStringLiteral("Added more than %1 cases — consider project_stats").arg(kAddCasesSoftWarn));
        return toolResultObj(out);
    }

    if (name == QLatin1String("add_gages")) {
        const QJsonArray gages = args.value(QStringLiteral("gages")).toArray();
        if (gages.isEmpty())
            return toolError(QStringLiteral("gages array required"));
        int added = 0;
        for (const QJsonValue& v : gages) {
            const QJsonObject g = v.toObject();
            const QString description = g.value(QStringLiteral("description")).toString().trimmed();
            if (description.isEmpty())
                continue;
            const int number = g.contains(QStringLiteral("number")) ? g.value(QStringLiteral("number")).toInt(1) : 1;
            const int hp = g.contains(QStringLiteral("hp")) ? g.value(QStringLiteral("hp")).toInt(5) : 5;
            const int rate = g.contains(QStringLiteral("rate")) ? g.value(QStringLiteral("rate")).toInt(100) : 100;
            // AppController::addGage(description, hp, number, rate)
            ctrl->addGage(description, hp, number, rate);
            ++added;
        }
        return toolResultObj({{QStringLiteral("added"), added},
                              {QStringLiteral("gagesTotal"), ctrl->gages().size()}});
    }

    if (name == QLatin1String("import_phrases_csv") || name == QLatin1String("import_gages_csv")) {
        QString err;
        const QString csv = readCsvArg(args, &err);
        if (!err.isEmpty())
            return toolError(err);
        const bool replace = args.value(QStringLiteral("replace")).toBool(false);
        const QVariantMap result = (name == QLatin1String("import_phrases_csv"))
            ? ctrl->importPhrasesCsvText(csv, replace)
            : ctrl->importGagesCsvText(csv, replace);
        QJsonObject out = variantMapToJson(result);
        out.insert(QStringLiteral("gridsDirty"), ctrl->gridsDirty());
        const bool okFlag = result.contains(QStringLiteral("ok"))
            ? result.value(QStringLiteral("ok")).toBool()
            : true;
        return toolResultObj(out, !okFlag);
    }

    if (name == QLatin1String("export_phrases_csv")) {
        return toolResultObj({{QStringLiteral("csv"), ctrl->exportPhrasesCsvText()}});
    }
    if (name == QLatin1String("export_gages_csv")) {
        return toolResultObj({{QStringLiteral("csv"), ctrl->exportGagesCsvText()}});
    }

    if (name == QLatin1String("clear_cases")) {
        if (!args.value(QStringLiteral("confirm")).toBool())
            return toolError(QStringLiteral("confirm=true required"));
        int removed = 0;
        while (!ctrl->cases().isEmpty()) {
            ctrl->removeCase(0);
            ++removed;
        }
        return toolResultObj({{QStringLiteral("removed"), removed},
                              {QStringLiteral("gridsDirty"), ctrl->gridsDirty()}});
    }
    if (name == QLatin1String("clear_gages")) {
        if (!args.value(QStringLiteral("confirm")).toBool())
            return toolError(QStringLiteral("confirm=true required"));
        int removed = 0;
        while (!ctrl->gages().isEmpty()) {
            ctrl->removeGage(0);
            ++removed;
        }
        return toolResultObj({{QStringLiteral("removed"), removed}});
    }

    if (name == QLatin1String("generate_grids")) {
        const QString msg = ctrl->generateAll();
        const bool failed = ctrl->grids().isEmpty() && ctrl->gridsDirty();
        return toolResultObj({{QStringLiteral("message"), msg},
                              {QStringLiteral("gridsCount"), ctrl->grids().size()},
                              {QStringLiteral("gridsDirty"), ctrl->gridsDirty()}},
                             failed);
    }

    if (name == QLatin1String("project_stats")) {
        return toolResultObj({{QStringLiteral("id"), ctrl->currentProjectId()},
                              {QStringLiteral("title"), ctrl->title()},
                              {QStringLiteral("minCases"), ctrl->minCases()},
                              {QStringLiteral("availableCells"), ctrl->availableCells()},
                              {QStringLiteral("gridsDirty"), ctrl->gridsDirty()},
                              {QStringLiteral("cases"), ctrl->cases().size()},
                              {QStringLiteral("gages"), ctrl->gages().size()},
                              {QStringLiteral("players"), ctrl->players().size()},
                              {QStringLiteral("grids"), ctrl->grids().size()},
                              {QStringLiteral("gridRows"), ctrl->gridRows()},
                              {QStringLiteral("gridCols"), ctrl->gridCols()},
                              {QStringLiteral("gageMode"), ctrl->gageMode()}});
    }

    if (name == QLatin1String("import_cues_as_cases")) {
        QStringList labels;
        if (args.contains(QStringLiteral("texts")) && args.value(QStringLiteral("texts")).isArray()) {
            for (const QJsonValue& v : args.value(QStringLiteral("texts")).toArray()) {
                const QString t = v.toString().trimmed();
                if (!t.isEmpty())
                    labels.append(t);
            }
        } else if (args.contains(QStringLiteral("indices"))
                   && args.value(QStringLiteral("indices")).isArray()) {
            if (g_lastCues.empty())
                return toolError(QStringLiteral("No cues cached — call preview_cues first"));
            for (const QJsonValue& v : args.value(QStringLiteral("indices")).toArray()) {
                const int idx = v.toInt(-1);
                if (idx < 0 || idx >= static_cast<int>(g_lastCues.size()))
                    continue;
                const core::Cue& c = g_lastCues[static_cast<size_t>(idx)];
                const QString t = QString::fromStdString(c.plain.empty() ? c.text : c.plain).trimmed();
                if (!t.isEmpty())
                    labels.append(t);
            }
        } else {
            return toolError(QStringLiteral("Provide texts[] or indices[]"));
        }
        if (labels.isEmpty())
            return toolError(QStringLiteral("No phrases to import"));

        int added = 0;
        for (const QString& label : labels) {
            ctrl->addCase(label, 1, 50);
            ++added;
        }
        QJsonObject out{{QStringLiteral("added"), added},
                        {QStringLiteral("casesTotal"), ctrl->cases().size()},
                        {QStringLiteral("gridsDirty"), ctrl->gridsDirty()}};
        if (added > kAddCasesSoftWarn)
            out.insert(QStringLiteral("warning"),
                       QStringLiteral("Added more than %1 cases — consider project_stats")
                           .arg(kAddCasesSoftWarn));
        return toolResultObj(out);
    }

    return toolError(QStringLiteral("Unknown tool: %1").arg(name));
}

QJsonObject dispatchRpc(AppController* ctrl, const QString& token, const QJsonObject& msg)
{
    const QJsonValue id = msg.value(QStringLiteral("id"));
    const QString method = msg.value(QStringLiteral("method")).toString();
    const QJsonObject params = msg.value(QStringLiteral("params")).toObject();

    // Notifications (no id): empty object signals "no JSON body" to caller.
    if (!msg.contains(QStringLiteral("id"))) {
        if (method == QLatin1String("notifications/initialized")
            || method == QLatin1String("notifications/cancelled"))
            return {};
        return {};
    }

    if (method == QLatin1String("initialize")) {
        const QString clientVersion = params.value(QStringLiteral("protocolVersion")).toString();
        const QString version = clientVersion.isEmpty() ? QStringLiteral("2024-11-05") : clientVersion;
        QJsonObject result;
        result.insert(QStringLiteral("protocolVersion"), version);
        result.insert(QStringLiteral("capabilities"),
                      QJsonObject{{QStringLiteral("tools"), QJsonObject{}}});
        result.insert(QStringLiteral("serverInfo"),
                      QJsonObject{{QStringLiteral("name"), QStringLiteral("openbingo")},
                                  {QStringLiteral("version"), QStringLiteral(BINGO_APP_VERSION)}});
        Q_UNUSED(token);
        return rpcResult(id, result);
    }

    if (method == QLatin1String("ping"))
        return rpcResult(id, QJsonObject{});

    if (method == QLatin1String("tools/list"))
        return rpcResult(id, QJsonObject{{QStringLiteral("tools"), toolsCatalog()}});

    if (method == QLatin1String("tools/call")) {
        const QString name = params.value(QStringLiteral("name")).toString();
        const QJsonObject arguments = params.value(QStringLiteral("arguments")).toObject();
        if (name.isEmpty())
            return rpcError(id, -32602, QStringLiteral("tools/call requires name"));
        return rpcResult(id, invokeTool(ctrl, name, arguments));
    }

    if (method == QLatin1String("resources/list"))
        return rpcResult(id, QJsonObject{{QStringLiteral("resources"), QJsonArray{}}});
    if (method == QLatin1String("prompts/list"))
        return rpcResult(id, QJsonObject{{QStringLiteral("prompts"), QJsonArray{}}});

    return rpcError(id, -32601, QStringLiteral("Method not found: %1").arg(method));
}

#endif // !Q_OS_ANDROID

} // namespace

McpServer::McpServer(AppController* controller, QObject* parent)
    : QObject(parent)
    , m_controller(controller)
{
    regenerateToken();
}

McpServer::~McpServer()
{
    stop();
}

QString McpServer::url() const
{
    if (!m_running)
        return {};
    return QStringLiteral("http://127.0.0.1:%1/mcp").arg(m_port);
}

QString McpServer::cursorConfigSnippet() const
{
    return QStringLiteral(
               "{\n"
               "  \"mcpServers\": {\n"
               "    \"openbingo\": {\n"
               "      \"url\": \"http://127.0.0.1:%1/mcp\",\n"
               "      \"headers\": {\n"
               "        \"Authorization\": \"Bearer %2\"\n"
               "      }\n"
               "    }\n"
               "  }\n"
               "}\n")
        .arg(m_port)
        .arg(m_token);
}

void McpServer::setEnabled(bool on)
{
    if (m_enabled == on)
        return;
    m_enabled = on;
    emit enabledChanged();
#ifdef Q_OS_ANDROID
    Q_UNUSED(on);
    return;
#else
    if (m_enabled)
        start();
    else
        stop();
#endif
}

void McpServer::setPort(int port)
{
    if (port < 1 || port > 65535 || port == m_port)
        return;
    const bool was = m_running;
    if (was)
        stop();
    m_port = port;
    emit portChanged();
    if (was && m_enabled)
        start();
}

void McpServer::regenerateToken()
{
    m_token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    emit tokenChanged();
}

void McpServer::copyConfigToClipboard()
{
    if (auto* clip = QGuiApplication::clipboard())
        clip->setText(cursorConfigSnippet());
}

void McpServer::start()
{
#ifdef Q_OS_ANDROID
    return;
#else
    if (m_running)
        return;
    if (!m_server) {
        m_server = new QTcpServer(this);
        QObject::connect(m_server, &QTcpServer::newConnection, this, &McpServer::onNewConnection);
    }
    if (m_server->isListening())
        m_server->close();
    if (!m_server->listen(QHostAddress::LocalHost, static_cast<quint16>(m_port))) {
        qWarning("McpServer: listen failed on 127.0.0.1:%d — %s", m_port,
                 qPrintable(m_server->errorString()));
        return;
    }
    m_running = true;
    emit runningChanged();
#endif
}

void McpServer::stop()
{
#ifdef Q_OS_ANDROID
    if (m_running) {
        m_running = false;
        emit runningChanged();
    }
    return;
#else
    if (m_server) {
        m_server->close();
        const auto clients = m_server->findChildren<QTcpSocket*>();
        for (QTcpSocket* s : clients)
            s->disconnectFromHost();
    }
    if (!m_running)
        return;
    m_running = false;
    emit runningChanged();
#endif
}

#ifndef Q_OS_ANDROID

void McpServer::onNewConnection()
{
    if (!m_server)
        return;
    while (QTcpSocket* sock = m_server->nextPendingConnection()) {
        sock->setParent(m_server);
        QObject::connect(sock, &QTcpSocket::readyRead, this, &McpServer::onSocketReadyRead);
        QObject::connect(sock, &QTcpSocket::disconnected, sock, &QObject::deleteLater);
    }
}

void McpServer::onSocketReadyRead()
{
    auto* sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock)
        return;
    QByteArray buf = sock->property("mcpBuf").toByteArray();
    buf.append(sock->readAll());

    QByteArray method, path, body;
    QList<QPair<QByteArray, QByteArray>> headers;
    if (!parseHttpRequest(buf, &method, &path, &headers, &body)) {
        sock->setProperty("mcpBuf", buf);
        // Guard against huge incomplete requests
        if (buf.size() > 8 * 1024 * 1024) {
            sock->write(httpResponse(413, "Payload Too Large",
                                     QByteArrayLiteral("{\"error\":\"too large\"}")));
            sock->disconnectFromHost();
        }
        return;
    }
    sock->setProperty("mcpBuf", QByteArray());

    // Reconstruct raw for handleHttp (needs auth etc.) — rebuild minimal raw.
    QByteArray raw = method + ' ' + path + " HTTP/1.1\r\n";
    for (const auto& h : headers)
        raw += h.first + ": " + h.second + "\r\n";
    raw += "\r\n";
    raw += body;

    const QByteArray resp = handleHttp(raw);
    sock->write(resp);
    sock->disconnectFromHost();
}

QByteArray McpServer::handleHttp(const QByteArray& raw) const
{
    QByteArray method, path, body;
    QList<QPair<QByteArray, QByteArray>> headers;
    if (!parseHttpRequest(raw, &method, &path, &headers, &body))
        return httpResponse(400, "Bad Request", QByteArrayLiteral("{\"error\":\"bad request\"}"));

    QByteArray pathOnly = path;
    const int q = pathOnly.indexOf('?');
    if (q >= 0)
        pathOnly = pathOnly.left(q);
    if (pathOnly != "/mcp" && pathOnly != "/mcp/")
        return httpResponse(404, "Not Found", QByteArrayLiteral("{\"error\":\"not found\"}"));

    if (method == "OPTIONS")
        return httpResponse(204, "No Content", {});

    if (method == "GET") {
        QJsonObject info{{QStringLiteral("name"), QStringLiteral("openbingo")},
                         {QStringLiteral("transport"), QStringLiteral("streamable-http")},
                         {QStringLiteral("path"), QStringLiteral("/mcp")},
                         {QStringLiteral("auth"), QStringLiteral("Bearer")}};
        return httpResponse(200, "OK", QJsonDocument(info).toJson(QJsonDocument::Compact));
    }

    if (method != "POST")
        return httpResponse(405, "Method Not Allowed",
                            QByteArrayLiteral("{\"error\":\"POST required\"}"));

    const QString auth = headerValue(headers, "Authorization");
    const QString expected = QStringLiteral("Bearer %1").arg(m_token);
    if (m_token.isEmpty() || auth != expected)
        return httpResponse(401, "Unauthorized",
                            QByteArrayLiteral("{\"error\":\"unauthorized\"}"));

    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(body, &pe);
    if (pe.error != QJsonParseError::NoError || doc.isNull())
        return httpResponse(400, "Bad Request",
                            QByteArrayLiteral("{\"error\":\"invalid json\"}"));

    // Batch
    if (doc.isArray()) {
        QJsonArray out;
        bool anyResponse = false;
        for (const QJsonValue& v : doc.array()) {
            if (!v.isObject())
                continue;
            const QJsonObject resp = dispatchRpc(m_controller, m_token, v.toObject());
            if (!resp.isEmpty()) {
                out.append(resp);
                anyResponse = true;
            }
        }
        if (!anyResponse)
            return httpResponse(202, "Accepted", {});
        return httpResponse(200, "OK", QJsonDocument(out).toJson(QJsonDocument::Compact));
    }

    if (!doc.isObject())
        return httpResponse(400, "Bad Request",
                            QByteArrayLiteral("{\"error\":\"invalid json-rpc\"}"));

    const QJsonObject msg = doc.object();
    // Notification → 202
    if (!msg.contains(QStringLiteral("id"))) {
        dispatchRpc(m_controller, m_token, msg);
        return httpResponse(202, "Accepted", {});
    }

    const QJsonObject resp = dispatchRpc(m_controller, m_token, msg);
    return httpResponse(200, "OK", QJsonDocument(resp).toJson(QJsonDocument::Compact));
}

#endif // !Q_OS_ANDROID

} // namespace app
