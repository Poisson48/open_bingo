#pragma once

#include "../core/bingotypes.h"

#include <QSqlDatabase>
#include <QString>
#include <optional>
#include <string>
#include <vector>

namespace store {

class Database
{
public:
    Database() = default;
    ~Database();

    bool open(const QString& path);
    void close();
    bool isOpen() const { return m_db.isOpen(); }

    bool upsertProject(const core::Project& project);
    bool deleteProject(const std::string& id);
    std::optional<core::Project> getProject(const std::string& id);
    std::vector<core::Project> getAllProjects();

    std::optional<std::string> getSetting(const std::string& key);
    bool setSetting(const std::string& key, const std::string& value);

    bool savePlayChecks(const std::string& projectId, const std::string& playerName,
                        const std::string& checksJson);
    std::optional<std::string> getPlayChecks(const std::string& projectId,
                                             const std::string& playerName);
    bool deletePlayChecksForProject(const std::string& projectId);

    // Sync Nostr (même stack que Colo) : clé stable une fois le partage activé.
    bool setSyncKey(const std::string& projectId, const std::vector<uint8_t>& key);
    std::optional<std::vector<uint8_t>> getSyncKey(const std::string& projectId);
    bool clearSyncKey(const std::string& projectId);
    std::vector<std::string> sharedProjectIds();
    bool outboxPush(const std::string& projectId, const std::string& eventId,
                    const std::string& content);
    bool outboxRemoveForEvent(const std::string& eventId);
    bool outboxRemoveForProject(const std::string& projectId);
    int outboxCount();
    struct OutboxRow { std::string projectId; std::string eventId; std::string content; };
    std::vector<OutboxRow> outboxPeekAll();
    bool markEventSeen(const std::string& eventId);
    bool isEventSeen(const std::string& eventId);

private:
    bool createSchema();

    QSqlDatabase m_db;
    QString      m_connectionName;
};

} // namespace store
