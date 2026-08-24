#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

namespace app::mcp {

QByteArray httpResponse(int status, const char* reason, const QByteArray& body,
                        const char* contentType = "application/json");

QString headerValue(const QList<QPair<QByteArray, QByteArray>>& headers, const QByteArray& name);

bool parseHttpRequest(const QByteArray& raw, QByteArray* method, QByteArray* path,
                      QList<QPair<QByteArray, QByteArray>>* headers, QByteArray* body);

QJsonObject rpcError(const QJsonValue& id, int code, const QString& message);
QJsonObject rpcResult(const QJsonValue& id, const QJsonValue& result);

QJsonObject toolResultObj(const QJsonObject& obj, bool isError = false);
QJsonObject toolError(const QString& message);

QJsonObject toolDef(const QString& name, const QString& description, const QJsonObject& inputSchema);
QJsonObject objectSchema(const QJsonObject& properties, const QStringList& required = {});

} // namespace app::mcp
