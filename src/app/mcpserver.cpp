#include "mcpserver.h"
#include "appcontroller.h"
#include "mcp_json.h"
#include "mcp_tools.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

#ifndef Q_OS_ANDROID
#  include <QHostAddress>
#  include <QTcpServer>
#  include <QTcpSocket>
#endif

namespace app {
namespace {

#ifndef Q_OS_ANDROID

using mcp::headerValue;
using mcp::httpResponse;
using mcp::parseHttpRequest;
using mcp::rpcError;
using mcp::rpcResult;

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
        return rpcResult(id, QJsonObject{{QStringLiteral("tools"), mcp::toolsCatalog()}});

    if (method == QLatin1String("tools/call")) {
        const QString name = params.value(QStringLiteral("name")).toString();
        const QJsonObject arguments = params.value(QStringLiteral("arguments")).toObject();
        if (name.isEmpty())
            return rpcError(id, -32602, QStringLiteral("tools/call requires name"));
        return rpcResult(id, mcp::invokeTool(ctrl, name, arguments));
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
