#pragma once

#include "../core/bingotypes.h"
#include "../store/database.h"

#include <QSet>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <map>
#include <string>
#include <vector>

namespace app::play {

using ChecksMatrix = std::vector<std::vector<bool>>;

ChecksMatrix checksFromVariant(const QVariantList& checks, int rows, int cols);
bool playChecksLookValid(const QVariantList& checks, int rows, int cols);
QVariantList checksToVariant(const ChecksMatrix& checks);
ChecksMatrix emptyChecks(int rows, int cols, bool freeCenter);

QVariantList buildScoreboard(const core::Project& project, store::Database* db);

QSet<QString> fullPlayersFromChecksMap(
    const core::Project& project,
    const std::map<std::string, std::string>& checksByPlayer);

QVariantList overlaysForNewlyCheckedCells(
    const core::Project& project,
    const std::map<std::string, std::string>& before,
    const std::map<std::string, std::string>& after);

// Persiste les coches dans db. N'émet aucun signal — AppController orchestre.
// result = map QML (checked, checks, overlays, winners, …).
// outboundOverlays = payload sync (vide si décochage).
struct ToggleOutcome {
    QVariantMap result;
    QVariantList newWinners;
    QVariantList scoreboard;
    QVariantList outboundOverlays;
};

ToggleOutcome togglePlayCell(core::Project& project, store::Database* db,
                             const QString& playerName, int row, int col);

void resetAllPlayChecks(core::Project& project, store::Database* db);

int computeScore(const core::Project& project, const QString& playerName,
                 const QVariantList& checks);
QVariantList detectBingoLines(const core::Project& project, const QVariantList& checks);

QString winnersToastMessage(const QVariantList& newWinners);

} // namespace app::play
