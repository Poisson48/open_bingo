#include "mcp_json.h"

#include <QJsonArray>
#include <QJsonDocument>

namespace app::mcp {

QByteArray httpResponse(int status, const char* reason, const QByteArray& body,
                        const char* contentType)
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

QJsonObject toolResultObj(const QJsonObject& obj, bool isError)
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

QJsonObject objectSchema(const QJsonObject& properties, const QStringList& required)
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

} // namespace app::mcp
