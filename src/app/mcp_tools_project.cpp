#include "mcp_tools.h"

#include "appcontroller.h"
#include "mcp_json.h"
#include "projectmodel.h"

#include <QFile>
#include <QJsonArray>
#include <QModelIndex>
#include <QVariantMap>

namespace app::mcp {
namespace {

constexpr int kPreviewLimit = 30;
constexpr int kAddCasesSoftWarn = 500;

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

} // namespace

QJsonObject invokeProjectTool(AppController* ctrl, const QString& name, const QJsonObject& args)
{
    if (!ctrl)
        return toolError(QStringLiteral("No controller"));

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

    if (name == QLatin1String("export_phrases_csv"))
        return toolResultObj({{QStringLiteral("csv"), ctrl->exportPhrasesCsvText()}});
    if (name == QLatin1String("export_gages_csv"))
        return toolResultObj({{QStringLiteral("csv"), ctrl->exportGagesCsvText()}});

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
            QString err;
            labels = labelsFromCachedCueIndices(args.value(QStringLiteral("indices")).toArray(), &err);
            if (!err.isEmpty())
                return toolError(err);
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

} // namespace app::mcp
