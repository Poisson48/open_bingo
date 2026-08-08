#include "database.h"

#include "../core/jsoncodec.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QDateTime>

namespace store {

Database::~Database()
{
    close();
}

bool Database::open(const QString& path)
{
    if (m_db.isOpen())
        close();

    m_connectionName = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_db.setDatabaseName(path);

    if (!m_db.open()) {
        qWarning("Database open failed: %s", qPrintable(m_db.lastError().text()));
        return false;
    }

    QSqlQuery pragma(m_db);
    pragma.exec(QStringLiteral("PRAGMA journal_mode=WAL"));

    return createSchema();
}

void Database::close()
{
    if (m_db.isOpen())
        m_db.close();
    if (!m_connectionName.isEmpty()) {
        QSqlDatabase::removeDatabase(m_connectionName);
        m_connectionName.clear();
    }
}

bool Database::createSchema()
{
    const QStringList statements = {
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS projects ("
            "id TEXT PRIMARY KEY, title TEXT NOT NULL, description TEXT,"
            "updated_at INTEGER NOT NULL, json TEXT NOT NULL)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS settings (key TEXT PRIMARY KEY, value TEXT)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS play_checks ("
            "project_id TEXT NOT NULL, player_name TEXT NOT NULL, checks_json TEXT NOT NULL,"
            "PRIMARY KEY (project_id, player_name))"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS sync_keys (project_id TEXT PRIMARY KEY, key_blob BLOB NOT NULL)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS outbox ("
            "event_id TEXT PRIMARY KEY, project_id TEXT NOT NULL, content TEXT NOT NULL)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS seen_events (event_id TEXT PRIMARY KEY, seen_at INTEGER NOT NULL)"),
    };

    QSqlQuery q(m_db);
    for (const QString& stmt : statements) {
        if (!q.exec(stmt)) {
            qWarning("createSchema failed: %s", qPrintable(q.lastError().text()));
            return false;
        }
    }
    return true;
}

bool Database::upsertProject(const core::Project& project)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO projects (id, title, description, updated_at, json) "
        "VALUES (?, ?, ?, ?, ?) "
        "ON CONFLICT(id) DO UPDATE SET title=excluded.title, description=excluded.description, "
        "updated_at=excluded.updated_at, json=excluded.json"));
    q.addBindValue(QString::fromStdString(project.id));
    q.addBindValue(QString::fromStdString(project.title));
    q.addBindValue(QString::fromStdString(project.description));
    q.addBindValue(static_cast<qlonglong>(project.updatedAt));
    q.addBindValue(QString::fromStdString(core::JsonCodec::projectToJson(project, false)));
    return q.exec();
}

bool Database::deleteProject(const std::string& id)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM projects WHERE id = ?"));
    q.addBindValue(QString::fromStdString(id));
    if (!q.exec())
        return false;
    deletePlayChecksForProject(id);
    return true;
}

std::optional<core::Project> Database::getProject(const std::string& id)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT json FROM projects WHERE id = ?"));
    q.addBindValue(QString::fromStdString(id));
    if (!q.exec() || !q.next())
        return std::nullopt;
    bool ok = false;
    const auto project = core::JsonCodec::projectFromJson(q.value(0).toString().toStdString(), &ok);
    return ok ? std::optional{ project } : std::nullopt;
}

std::vector<core::Project> Database::getAllProjects()
{
    std::vector<core::Project> out;
    QSqlQuery q(m_db);
    q.exec(QStringLiteral("SELECT json FROM projects ORDER BY updated_at DESC"));
    while (q.next()) {
        bool ok = false;
        auto p = core::JsonCodec::projectFromJson(q.value(0).toString().toStdString(), &ok);
        if (ok)
            out.push_back(std::move(p));
    }
    return out;
}

std::optional<std::string> Database::getSetting(const std::string& key)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT value FROM settings WHERE key = ?"));
    q.addBindValue(QString::fromStdString(key));
    if (!q.exec() || !q.next())
        return std::nullopt;
    return q.value(0).toString().toStdString();
}

bool Database::setSetting(const std::string& key, const std::string& value)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO settings (key, value) VALUES (?, ?) "
        "ON CONFLICT(key) DO UPDATE SET value=excluded.value"));
    q.addBindValue(QString::fromStdString(key));
    q.addBindValue(QString::fromStdString(value));
    return q.exec();
}

bool Database::savePlayChecks(const std::string& projectId, const std::string& playerName,
                              const std::string& checksJson)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO play_checks (project_id, player_name, checks_json) VALUES (?, ?, ?) "
        "ON CONFLICT(project_id, player_name) DO UPDATE SET checks_json=excluded.checks_json"));
    q.addBindValue(QString::fromStdString(projectId));
    q.addBindValue(QString::fromStdString(playerName));
    q.addBindValue(QString::fromStdString(checksJson));
    return q.exec();
}

std::optional<std::string> Database::getPlayChecks(const std::string& projectId,
                                                   const std::string& playerName)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT checks_json FROM play_checks WHERE project_id = ? AND player_name = ?"));
    q.addBindValue(QString::fromStdString(projectId));
    q.addBindValue(QString::fromStdString(playerName));
    if (!q.exec() || !q.next())
        return std::nullopt;
    return q.value(0).toString().toStdString();
}

bool Database::deletePlayChecksForProject(const std::string& projectId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM play_checks WHERE project_id = ?"));
    q.addBindValue(QString::fromStdString(projectId));
    if (!q.exec())
        return false;
    QSqlQuery q2(m_db);
    q2.prepare(QStringLiteral("DELETE FROM sync_keys WHERE project_id = ?"));
    q2.addBindValue(QString::fromStdString(projectId));
    q2.exec();
    return true;
}

bool Database::setSyncKey(const std::string& projectId, const std::vector<uint8_t>& key)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO sync_keys (project_id, key_blob) VALUES (?, ?) "
        "ON CONFLICT(project_id) DO UPDATE SET key_blob=excluded.key_blob"));
    q.addBindValue(QString::fromStdString(projectId));
    q.addBindValue(QByteArray(reinterpret_cast<const char*>(key.data()),
                              static_cast<int>(key.size())));
    return q.exec();
}

std::optional<std::vector<uint8_t>> Database::getSyncKey(const std::string& projectId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT key_blob FROM sync_keys WHERE project_id = ?"));
    q.addBindValue(QString::fromStdString(projectId));
    if (!q.exec() || !q.next())
        return std::nullopt;
    const QByteArray blob = q.value(0).toByteArray();
    return std::vector<uint8_t>(blob.begin(), blob.end());
}

bool Database::clearSyncKey(const std::string& projectId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM sync_keys WHERE project_id = ?"));
    q.addBindValue(QString::fromStdString(projectId));
    return q.exec();
}

std::vector<std::string> Database::sharedProjectIds()
{
    std::vector<std::string> ids;
    QSqlQuery q(m_db);
    q.exec(QStringLiteral("SELECT project_id FROM sync_keys"));
    while (q.next())
        ids.push_back(q.value(0).toString().toStdString());
    return ids;
}

bool Database::outboxPush(const std::string& projectId, const std::string& eventId,
                          const std::string& content)
{
    // Une seule snapshot en attente par projet (évite les orphelins si re-sign change l'id).
    outboxRemoveForProject(projectId);
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO outbox (event_id, project_id, content) VALUES (?, ?, ?)"));
    q.addBindValue(QString::fromStdString(eventId));
    q.addBindValue(QString::fromStdString(projectId));
    q.addBindValue(QString::fromStdString(content));
    return q.exec();
}

bool Database::outboxRemoveForEvent(const std::string& eventId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM outbox WHERE event_id = ?"));
    q.addBindValue(QString::fromStdString(eventId));
    return q.exec();
}

bool Database::outboxRemoveForProject(const std::string& projectId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM outbox WHERE project_id = ?"));
    q.addBindValue(QString::fromStdString(projectId));
    return q.exec();
}

int Database::outboxCount()
{
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("SELECT COUNT(*) FROM outbox")) || !q.next())
        return 0;
    return q.value(0).toInt();
}

std::vector<Database::OutboxRow> Database::outboxPeekAll()
{
    std::vector<OutboxRow> rows;
    QSqlQuery q(m_db);
    q.exec(QStringLiteral("SELECT project_id, event_id, content FROM outbox"));
    while (q.next()) {
        rows.push_back({
            q.value(0).toString().toStdString(),
            q.value(1).toString().toStdString(),
            q.value(2).toString().toStdString(),
        });
    }
    return rows;
}

bool Database::markEventSeen(const std::string& eventId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO seen_events (event_id, seen_at) VALUES (?, ?)"));
    q.addBindValue(QString::fromStdString(eventId));
    q.addBindValue(QDateTime::currentMSecsSinceEpoch());
    return q.exec();
}

bool Database::isEventSeen(const std::string& eventId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT 1 FROM seen_events WHERE event_id = ?"));
    q.addBindValue(QString::fromStdString(eventId));
    return q.exec() && q.next();
}

} // namespace store
