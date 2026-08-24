#include "mcp_tools.h"

#include "mcp_json.h"

#include <QJsonArray>

namespace app::mcp {

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

QJsonObject invokeTool(AppController* ctrl, const QString& name, const QJsonObject& args)
{
    if (!ctrl)
        return toolError(QStringLiteral("No controller"));

    // Film tools (no project required except import_cues_as_cases).
    if (name == QLatin1String("search_subtitles") || name == QLatin1String("download_subtitle")
        || name == QLatin1String("preview_cues") || name == QLatin1String("suggest_bingo_phrases")) {
        return invokeFilmTool(ctrl, name, args);
    }

    return invokeProjectTool(ctrl, name, args);
}

} // namespace app::mcp
