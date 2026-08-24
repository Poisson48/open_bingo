#pragma once

#include <QObject>
#include <QString>

class QTcpServer;

namespace app {

class AppController;

// MCP Streamable HTTP (localhost only). Desktop only — no-op / not started on Android.
// Plan: docs/PLAN-csv-mcp.md § Phase B.
//
// Tools v1:
//   list_projects, get_project, create_project, open_project, set_config,
//   add_cases, add_gages, import_phrases_csv, import_gages_csv,
//   export_phrases_csv, export_gages_csv, clear_cases, clear_gages,
//   generate_grids, project_stats
// Tools film (PLAN-opensubtitles):
//   search_subtitles, download_subtitle, preview_cues,
//   suggest_bingo_phrases, import_cues_as_cases
//
// Default bind: 127.0.0.1:4546  path: /mcp
class McpServer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(int port READ port WRITE setPort NOTIFY portChanged)
    Q_PROPERTY(QString token READ token NOTIFY tokenChanged)
    Q_PROPERTY(QString url READ url NOTIFY runningChanged)
    Q_PROPERTY(QString cursorConfigSnippet READ cursorConfigSnippet NOTIFY tokenChanged)

public:
    explicit McpServer(AppController* controller, QObject* parent = nullptr);
    ~McpServer() override;

    bool enabled() const { return m_enabled; }
    bool running() const { return m_running; }
    int port() const { return m_port; }
    QString token() const { return m_token; }
    QString url() const;
    QString cursorConfigSnippet() const;

    void setEnabled(bool on);
    void setPort(int port);
    Q_INVOKABLE void regenerateToken();
    Q_INVOKABLE void copyConfigToClipboard();

signals:
    void enabledChanged();
    void runningChanged();
    void portChanged();
    void tokenChanged();

private:
    void start();
    void stop();

#ifndef Q_OS_ANDROID
    void onNewConnection();
    void onSocketReadyRead();
    QByteArray handleHttp(const QByteArray& raw) const;
#endif

    AppController* m_controller = nullptr;
    bool m_enabled = false;
    bool m_running = false;
    int m_port = 4546;
    QString m_token;
#ifndef Q_OS_ANDROID
    QTcpServer* m_server = nullptr;
#endif
};

} // namespace app
