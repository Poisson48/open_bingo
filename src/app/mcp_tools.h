#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace app {

class AppController;

namespace mcp {

QJsonArray toolsCatalog();

// Dispatches to project / csv / film handlers.
QJsonObject invokeTool(AppController* ctrl, const QString& name, const QJsonObject& args);

QJsonObject invokeFilmTool(AppController* ctrl, const QString& name, const QJsonObject& args);
QJsonObject invokeProjectTool(AppController* ctrl, const QString& name, const QJsonObject& args);

// Session cache for preview indices → import_cues_as_cases (film module).
QStringList labelsFromCachedCueIndices(const QJsonArray& indices, QString* error);

} // namespace mcp
} // namespace app
