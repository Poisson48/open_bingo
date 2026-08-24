#include "playlogic.h"

#include "../core/generator.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QStringList>
#include <QtGlobal>

#include <algorithm>
#include <map>
#include <random>
#include <set>

namespace app::play {

ChecksMatrix checksFromVariant(const QVariantList& checks, int rows, int cols)
{
    ChecksMatrix out(rows, std::vector<bool>(cols, false));
    // Grille à plat (bug / vieux JSON) : refuser plutôt que lire n'importe quoi.
    if (!checks.isEmpty() && !checks[0].canConvert<QVariantList>())
        return out;
    for (int r = 0; r < rows && r < checks.size(); ++r) {
        const QVariantList row = checks[r].toList();
        for (int c = 0; c < cols && c < row.size(); ++c)
            out[static_cast<size_t>(r)][static_cast<size_t>(c)] = row[c].toBool();
    }
    return out;
}

bool playChecksLookValid(const QVariantList& checks, int rows, int cols)
{
    if (rows <= 0 || cols <= 0)
        return false;
    if (checks.size() != rows)
        return false;
    for (int r = 0; r < rows; ++r) {
        if (!checks[r].canConvert<QVariantList>())
            return false;
        if (checks[r].toList().size() != cols)
            return false;
    }
    return true;
}

QVariantList checksToVariant(const ChecksMatrix& checks)
{
    QVariantList out;
    for (const auto& row : checks) {
        QVariantList r;
        for (bool v : row)
            r.append(v);
        // QVariantList::append(QVariantList) aplatit — encapsuler comme ailleurs.
        out.append(QVariant(r));
    }
    return out;
}

ChecksMatrix emptyChecks(int rows, int cols, bool freeCenter)
{
    ChecksMatrix out(rows, std::vector<bool>(cols, false));
    if (freeCenter && rows % 2 == 1 && cols % 2 == 1) {
        const int midR = rows / 2;
        const int midC = cols / 2;
        out[static_cast<size_t>(midR)][static_cast<size_t>(midC)] = true;
    }
    return out;
}

namespace {

QString normalizeCellLabel(const std::string& label)
{
    return QString::fromStdString(label).trimmed();
}

QVariantMap gageOverlayForCell(const core::Project& project, const core::GridCell& cell,
                               const QString& playerName)
{
    QVariantMap m;
    if (cell.isFree)
        return m;
    QString desc;
    int num = cell.points;
    int rateShown = 100;
    if (!cell.gage.empty()) {
        desc = QString::fromStdString(cell.gage);
    } else if (project.gageMode && num > 0) {
        // Tirage pondéré parmi les gages partageant ce n°.
        // Seed stable (projet + joueur + libellé + n°) : même résultat sur tous
        // les appareils quand la coche arrive via sync.
        std::vector<size_t> idxs;
        int total = 0;
        for (size_t i = 0; i < project.gages.size(); ++i) {
            const auto& g = project.gages[i];
            if (g.number != num)
                continue;
            idxs.push_back(i);
            total += qMax(0, g.rate);
        }
        if (idxs.empty())
            return m;
        size_t pick = idxs[0];
        if (idxs.size() > 1) {
            const QByteArray seedBytes = (QString::fromStdString(project.id) + QLatin1Char('|')
                                         + QString::fromStdString(cell.label) + QLatin1Char('|')
                                         + QString::number(num))
                                            .toUtf8();
            const quint64 seed = qHash(seedBytes, /*seed*/ quint64(0x0b1660u));
            std::mt19937 eng{ static_cast<std::mt19937::result_type>(seed) };
            if (total <= 0) {
                std::uniform_int_distribution<size_t> dist(0, idxs.size() - 1);
                pick = idxs[dist(eng)];
            } else {
                std::uniform_int_distribution<int> dist(1, total);
                int roll = dist(eng);
                int acc = 0;
                for (size_t i : idxs) {
                    acc += qMax(0, project.gages[i].rate);
                    if (roll <= acc) {
                        pick = i;
                        break;
                    }
                }
            }
        }
        const auto& g = project.gages[pick];
        desc = QString::fromStdString(g.description);
        rateShown = g.rate;
        // Compter combien de variantes pour ce n°
        m.insert(QStringLiteral("variants"), static_cast<int>(idxs.size()));
    }
    if (desc.isEmpty())
        return m;
    m.insert(QStringLiteral("kind"), QStringLiteral("gage"));
    m.insert(QStringLiteral("num"), num);
    m.insert(QStringLiteral("label"), QString::fromStdString(cell.label));
    m.insert(QStringLiteral("desc"), desc);
    m.insert(QStringLiteral("player"), playerName);
    m.insert(QStringLiteral("rate"), rateShown);
    return m;
}

QString formatPlayerList(const QStringList& names)
{
    if (names.isEmpty())
        return {};
    if (names.size() == 1)
        return names[0];
    if (names.size() == 2)
        return names[0] + QStringLiteral(" et ") + names[1];
    QStringList head = names.mid(0, names.size() - 1);
    return head.join(QStringLiteral(", ")) + QStringLiteral(" et ") + names.last();
}

// Regroupe gages / combos : une notif par événement (libellé coché),
// pas une par variante de tirage — sinon « moi » puis « les autres ».
QVariantList groupPlayOverlays(const QVariantList& raw)
{
    QList<QVariantMap> groups;
    QHash<QString, int> indexByKey;

    auto appendPlayer = [&](QVariantMap ov, const QString& groupKey) {
        const QString player = ov.value(QStringLiteral("player")).toString();
        const QString desc = ov.value(QStringLiteral("desc")).toString().trimmed();
        if (player.isEmpty() || desc.isEmpty())
            return;
        if (!indexByKey.contains(groupKey)) {
            QStringList players;
            players << player;
            ov.insert(QStringLiteral("players"), players);
            ov.remove(QStringLiteral("player"));
            indexByKey.insert(groupKey, groups.size());
            groups.append(ov);
        } else {
            QVariantMap& g = groups[indexByKey.value(groupKey)];
            QStringList players = g.value(QStringLiteral("players")).toStringList();
            if (!players.contains(player)) {
                players << player;
                g.insert(QStringLiteral("players"), players);
            }
        }
    };

    for (const QVariant& v : raw) {
        const QVariantMap ov = v.toMap();
        const QString kind = ov.value(QStringLiteral("kind")).toString();
        const QString desc = ov.value(QStringLiteral("desc")).toString().trimmed();
        if (kind == QLatin1String("gage")) {
            // Même case cochée (libellé) → une seule carte pour tout le monde.
            const QString label = ov.value(QStringLiteral("label")).toString().trimmed();
            appendPlayer(ov, QStringLiteral("gage\n") + label.toLower());
        } else if (kind == QLatin1String("combo")) {
            // Même type de combo + même texte → une carte.
            appendPlayer(ov, QStringLiteral("combo\n")
                                 + ov.value(QStringLiteral("key")).toString()
                                 + QLatin1Char('\n') + desc);
        }
    }

    QVariantList out;
    for (QVariantMap g : groups) {
        const QStringList players = g.value(QStringLiteral("players")).toStringList();
        const QString who = formatPlayerList(players);
        const QString verb = players.size() <= 1 ? QStringLiteral("doit")
                                                 : QStringLiteral("doivent");
        g.insert(QStringLiteral("who"), who);
        g.insert(QStringLiteral("verb"), verb);
        g.insert(QStringLiteral("prompt"),
                 who + QLatin1Char(' ') + verb + QStringLiteral(" :"));
        if (!players.isEmpty())
            g.insert(QStringLiteral("player"), players.join(QStringLiteral(", ")));
        out.append(g);
    }
    return out;
}

std::set<QString> bingoTypeSet(const core::Project& project,
                               const std::vector<std::vector<bool>>& checks)
{
    std::set<QString> types;
    const int rows = project.gridRows;
    const int cols = project.gridCols;
    const auto lines = core::detectBingo(checks, rows, cols);
    for (const auto& line : lines) {
        if (line.empty())
            continue;
        bool sameRow = true;
        bool sameCol = true;
        bool mainDiag = true;
        bool antiDiag = true;
        const int r0 = line[0].first;
        const int c0 = line[0].second;
        for (const auto& [r, c] : line) {
            if (r != r0) sameRow = false;
            if (c != c0) sameCol = false;
            if (r != c) mainDiag = false;
            if (rows == cols) {
                if (r + c != rows - 1) antiDiag = false;
            } else {
                antiDiag = false;
                mainDiag = false;
            }
        }
        if (sameRow)
            types.insert(QStringLiteral("line"));
        else if (sameCol)
            types.insert(QStringLiteral("column"));
        else if ((mainDiag || antiDiag) && rows == cols)
            types.insert(QStringLiteral("diagonal"));
    }
    return types;
}

int countPlayableCells(const core::PlayerGrid& grid)
{
    int n = 0;
    for (const auto& row : grid.cells)
        for (const auto& cell : row)
            if (!cell.isFree)
                ++n;
    return n;
}

int countCheckedPlayable(const core::PlayerGrid& grid,
                         const std::vector<std::vector<bool>>& checks)
{
    int n = 0;
    const int rows = static_cast<int>(grid.cells.size());
    for (int r = 0; r < rows; ++r) {
        const auto& crow = grid.cells[static_cast<size_t>(r)];
        for (int c = 0; c < static_cast<int>(crow.size()); ++c) {
            if (crow[static_cast<size_t>(c)].isFree)
                continue;
            if (r < static_cast<int>(checks.size())
                && c < static_cast<int>(checks[static_cast<size_t>(r)].size())
                && checks[static_cast<size_t>(r)][static_cast<size_t>(c)])
                ++n;
        }
    }
    return n;
}


} // namespace

QVariantList buildScoreboard(const core::Project& project, store::Database* db)
{
    QVariantList board;
    if (!db)
        return board;
    const int rows = project.gridRows;
    const int cols = project.gridCols;
    // Mode gage : les « points » des cases sont des n° de gage — on classe
    // sur les cases cochées, pas sur la somme des n°.
    const bool gageMode = project.gageMode;
    for (const auto& grid : project.grids) {
        auto checks = emptyChecks(rows, cols, core::projectHasFreeCenter(project));
        if (const auto json = db->getPlayChecks(project.id, grid.player)) {
            const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(*json));
            checks = checksFromVariant(doc.array().toVariantList(), rows, cols);
            if (core::projectHasFreeCenter(project)) {
                const int midR = rows / 2;
                const int midC = cols / 2;
                checks[static_cast<size_t>(midR)][static_cast<size_t>(midC)] = true;
            }
        }
        const int total = countPlayableCells(grid);
        const int checked = countCheckedPlayable(grid, checks);
        const bool full = total > 0 && checked >= total;
        const int score = gageMode ? checked : core::computeScore(grid, checks);
        board.append(QVariantMap{
            { QStringLiteral("player"), QString::fromStdString(grid.player) },
            { QStringLiteral("score"), score },
            { QStringLiteral("checked"), checked },
            { QStringLiteral("total"), total },
            { QStringLiteral("full"), full },
            { QStringLiteral("gageMode"), gageMode },
            { QStringLiteral("unit"), gageMode ? QStringLiteral("cases")
                                              : QStringLiteral("pts") },
        });
    }
    std::sort(board.begin(), board.end(), [](const QVariant& a, const QVariant& b) {
        const auto ma = a.toMap();
        const auto mb = b.toMap();
        if (ma.value(QStringLiteral("full")).toBool() != mb.value(QStringLiteral("full")).toBool())
            return ma.value(QStringLiteral("full")).toBool();
        const int sa = ma.value(QStringLiteral("score")).toInt();
        const int sb = mb.value(QStringLiteral("score")).toInt();
        if (sa != sb)
            return sa > sb;
        return ma.value(QStringLiteral("checked")).toInt()
            > mb.value(QStringLiteral("checked")).toInt();
    });
    return board;
}


QSet<QString> fullPlayersFromChecksMap(
    const core::Project& project,
    const std::map<std::string, std::string>& checksByPlayer)
{
    QSet<QString> out;
    const int rows = project.gridRows;
    const int cols = project.gridCols;
    if (rows <= 0 || cols <= 0)
        return out;
    for (const auto& grid : project.grids) {
        auto checks = emptyChecks(rows, cols, core::projectHasFreeCenter(project));
        const auto it = checksByPlayer.find(grid.player);
        if (it != checksByPlayer.end()) {
            const QJsonDocument doc =
                QJsonDocument::fromJson(QByteArray::fromStdString(it->second));
            checks = checksFromVariant(doc.array().toVariantList(), rows, cols);
            if (core::projectHasFreeCenter(project)) {
                const int midR = rows / 2;
                const int midC = cols / 2;
                checks[static_cast<size_t>(midR)][static_cast<size_t>(midC)] = true;
            }
        }
        const int total = countPlayableCells(grid);
        const int checked = countCheckedPlayable(grid, checks);
        if (total > 0 && checked >= total)
            out.insert(QString::fromStdString(grid.player));
    }
    return out;
}

QVariantList overlaysForNewlyCheckedCells(
    const core::Project& project,
    const std::map<std::string, std::string>& before,
    const std::map<std::string, std::string>& after)
{
    QVariantList raw;
    if (!project.gageMode)
        return raw;
    const int rows = project.gridRows;
    const int cols = project.gridCols;
    if (rows <= 0 || cols <= 0)
        return raw;

    auto parse = [&](const std::string& player,
                     const std::map<std::string, std::string>& map) {
        const auto it = map.find(player);
        if (it == map.end())
            return emptyChecks(rows, cols, core::projectHasFreeCenter(project));
        const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(it->second));
        auto checks = checksFromVariant(doc.array().toVariantList(), rows, cols);
        if (static_cast<int>(checks.size()) != rows)
            return emptyChecks(rows, cols, core::projectHasFreeCenter(project));
        if (core::projectHasFreeCenter(project)) {
            const int midR = rows / 2;
            const int midC = cols / 2;
            checks[static_cast<size_t>(midR)][static_cast<size_t>(midC)] = true;
        }
        return checks;
    };

    QSet<QString> gagePlayersSeen;
    const auto& combos = project.comboGages;
    auto pushCombo = [&](const QString& pname, const char* key, const std::string& text,
                         const std::set<QString>& oldTypes, const std::set<QString>& newTypes) {
        if (text.empty())
            return;
        const QString k = QString::fromUtf8(key);
        if (!newTypes.count(k) || oldTypes.count(k))
            return;
        QVariantMap ov;
        ov.insert(QStringLiteral("kind"), QStringLiteral("combo"));
        ov.insert(QStringLiteral("key"), k);
        ov.insert(QStringLiteral("label"),
                  k == QLatin1String("line") ? QStringLiteral("Ligne complète")
                  : k == QLatin1String("column") ? QStringLiteral("Colonne complète")
                                                 : QStringLiteral("Diagonale complète"));
        ov.insert(QStringLiteral("desc"), QString::fromStdString(text));
        ov.insert(QStringLiteral("player"), pname);
        raw.append(ov);
    };

    for (const auto& grid : project.grids) {
        const QString pname = QString::fromStdString(grid.player);
        const auto wasChecks = parse(grid.player, before);
        const auto nowChecks = parse(grid.player, after);
        const auto oldTypes = bingoTypeSet(project, wasChecks);
        const auto newTypes = bingoTypeSet(project, nowChecks);

        for (int r = 0; r < rows && r < static_cast<int>(grid.cells.size()); ++r) {
            const auto& crow = grid.cells[static_cast<size_t>(r)];
            for (int c = 0; c < cols && c < static_cast<int>(crow.size()); ++c) {
                const auto& cell = crow[static_cast<size_t>(c)];
                if (cell.isFree)
                    continue;
                const bool was = wasChecks[static_cast<size_t>(r)][static_cast<size_t>(c)];
                const bool now = nowChecks[static_cast<size_t>(r)][static_cast<size_t>(c)];
                if (!now || was)
                    continue;
                if (gagePlayersSeen.contains(pname))
                    continue;
                QVariantMap ov = gageOverlayForCell(project, cell, pname);
                if (ov.isEmpty())
                    continue;
                gagePlayersSeen.insert(pname);
                raw.append(ov);
            }
        }

        pushCombo(pname, "line", combos.line, oldTypes, newTypes);
        pushCombo(pname, "column", combos.column, oldTypes, newTypes);
        pushCombo(pname, "diagonal", combos.diagonal, oldTypes, newTypes);
    }
    return groupPlayOverlays(raw);
}

ToggleOutcome togglePlayCell(core::Project& project, store::Database* db,
                             const QString& playerName, int row, int col)
{
    // Pattern Colo Courses/Tâches (ItemModel::toggleDone) : une seule API de cochage,
    // persistance immédiate, signal pour rafraîchir l'UI. Ici l'identité partagée
    // n'est pas un itemId mais le libellé de case (même film → même événement).
    ToggleOutcome out;
    QVariantMap& result = out.result;
    result.insert(QStringLiteral("checked"), false);
    result.insert(QStringLiteral("checks"), QVariantList{});
    result.insert(QStringLiteral("overlays"), QVariantList{});

    if (!db || playerName.isEmpty())
        return out;
    core::normalizeGridDims(project);
    const int rows = project.gridRows;
    const int cols = project.gridCols;
    if (rows <= 0 || cols <= 0 || row < 0 || col < 0 || row >= rows || col >= cols)
        return out;

    const core::PlayerGrid* sourceGrid = nullptr;
    for (const auto& g : project.grids) {
        if (g.player == playerName.toStdString()) {
            sourceGrid = &g;
            break;
        }
    }
    if (!sourceGrid || row >= static_cast<int>(sourceGrid->cells.size())
        || col >= static_cast<int>(sourceGrid->cells[static_cast<size_t>(row)].size()))
        return out;

    const auto& sourceCell = sourceGrid->cells[static_cast<size_t>(row)][static_cast<size_t>(col)];
    if (sourceCell.isFree)
        return out;

    const QString label = normalizeCellLabel(sourceCell.label);
    if (label.isEmpty())
        return out;

    auto loadOrEmpty = [&](const std::string& pname) {
        const auto json = db->getPlayChecks(project.id, pname);
        if (!json)
            return emptyChecks(rows, cols, core::projectHasFreeCenter(project));
        const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(*json));
        const QVariantList raw = doc.array().toVariantList();
        // Matrice corrompue / à plat : ne pas repartir de zéro (effacerait les coches).
        if (!playChecksLookValid(raw, rows, cols)) {
            qWarning("play checks invalides pour %s — conservation d'une grille vide non écrasante",
                     pname.c_str());
            // Tenter une lecture partielle plutôt qu'un wipe total.
        }
        auto checks = checksFromVariant(raw, rows, cols);
        if (core::projectHasFreeCenter(project)) {
            const int midR = rows / 2;
            const int midC = cols / 2;
            checks[static_cast<size_t>(midR)][static_cast<size_t>(midC)] = true;
        }
        return checks;
    };

    auto viewerChecksBefore = loadOrEmpty(playerName.toStdString());
    const bool newChecked = !viewerChecksBefore[static_cast<size_t>(row)][static_cast<size_t>(col)];

    // Combos avant cochage — pour chaque joueur (ligne / colonne / diagonale).
    std::map<std::string, std::set<QString>> comboTypesBefore;
    for (const auto& grid : project.grids)
        comboTypesBefore[grid.player] = bingoTypeSet(project, loadOrEmpty(grid.player));

    // Qui avait déjà une grille pleine (pour détecter les nouveaux gagnants).
    QSet<QString> fullBefore;
    for (const QVariant& rowV : buildScoreboard(project, db)) {
        const auto m = rowV.toMap();
        if (m.value(QStringLiteral("full")).toBool())
            fullBefore.insert(m.value(QStringLiteral("player")).toString());
    }

    QVariantList overlays;
    QVariantList viewerChecksOut;
    // Un overlay gage par joueur touché par le libellé (pas seulement celui
    // qui a tapé) — les autres doivent aussi faire leur gage.
    QSet<QString> gagePlayersSeen;

    for (const auto& grid : project.grids) {
        auto checks = loadOrEmpty(grid.player);
        const QString pname = QString::fromStdString(grid.player);

        for (int r = 0; r < rows && r < static_cast<int>(grid.cells.size()); ++r) {
            const auto& crow = grid.cells[static_cast<size_t>(r)];
            for (int c = 0; c < cols && c < static_cast<int>(crow.size()); ++c) {
                const auto& cell = crow[static_cast<size_t>(c)];
                if (cell.isFree)
                    continue;
                if (normalizeCellLabel(cell.label) != label)
                    continue;
                const bool was = checks[static_cast<size_t>(r)][static_cast<size_t>(c)];
                checks[static_cast<size_t>(r)][static_cast<size_t>(c)] = newChecked;
                if (newChecked && !was && project.gageMode
                    && !gagePlayersSeen.contains(pname)) {
                    QVariantMap ov = gageOverlayForCell(project, cell, pname);
                    if (!ov.isEmpty()) {
                        gagePlayersSeen.insert(pname);
                        const bool isTap = (grid.player == playerName.toStdString()
                                            && r == row && c == col);
                        if (isTap)
                            overlays.insert(0, ov);
                        else
                            overlays.append(ov);
                    }
                }
            }
        }

        if (core::projectHasFreeCenter(project)) {
            const int midR = rows / 2;
            const int midC = cols / 2;
            checks[static_cast<size_t>(midR)][static_cast<size_t>(midC)] = true;
        }

        const QVariantList asVar = checksToVariant(checks);
        db->savePlayChecks(project.id, grid.player,
                             QJsonDocument(QJsonArray::fromVariantList(asVar))
                                 .toJson(QJsonDocument::Compact)
                                 .toStdString());
        if (grid.player == playerName.toStdString())
            viewerChecksOut = asVar;
    }

    // Combos nouvellement débloqués chez n'importe quel joueur.
    if (newChecked && project.gageMode) {
        const auto& combos = project.comboGages;
        auto pushCombo = [&](const QString& pname, const char* key, const std::string& text,
                             const std::set<QString>& oldTypes, const std::set<QString>& newTypes) {
            if (text.empty())
                return;
            const QString k = QString::fromUtf8(key);
            if (!newTypes.count(k) || oldTypes.count(k))
                return;
            QVariantMap ov;
            ov.insert(QStringLiteral("kind"), QStringLiteral("combo"));
            ov.insert(QStringLiteral("key"), k);
            ov.insert(QStringLiteral("label"),
                      k == QLatin1String("line") ? QStringLiteral("Ligne complète")
                      : k == QLatin1String("column") ? QStringLiteral("Colonne complète")
                                                     : QStringLiteral("Diagonale complète"));
            ov.insert(QStringLiteral("desc"), QString::fromStdString(text));
            ov.insert(QStringLiteral("player"), pname);
            overlays.append(ov);
        };
        for (const auto& grid : project.grids) {
            const auto after = loadOrEmpty(grid.player);
            const auto newTypes = bingoTypeSet(project, after);
            const auto& oldTypes = comboTypesBefore[grid.player];
            const QString pname = QString::fromStdString(grid.player);
            pushCombo(pname, "line", combos.line, oldTypes, newTypes);
            pushCombo(pname, "column", combos.column, oldTypes, newTypes);
            pushCombo(pname, "diagonal", combos.diagonal, oldTypes, newTypes);
        }
    }

    const auto board = buildScoreboard(project, db);
    bool viewerFull = false;
    QVariantList winners;
    QVariantList newWinners;
    for (const QVariant& rowV : board) {
        const auto m = rowV.toMap();
        if (!m.value(QStringLiteral("full")).toBool())
            continue;
        const QString pname = m.value(QStringLiteral("player")).toString();
        winners.append(pname);
        if (pname == playerName)
            viewerFull = true;
        if (newChecked && !fullBefore.contains(pname))
            newWinners.append(m); // map complète (player, score, …)
    }

    result.insert(QStringLiteral("checked"), newChecked);
    result.insert(QStringLiteral("checks"), viewerChecksOut);
    const QVariantList groupedOverlays = groupPlayOverlays(overlays);
    result.insert(QStringLiteral("overlays"), groupedOverlays);
    result.insert(QStringLiteral("label"), label);
    result.insert(QStringLiteral("gridFull"), viewerFull);
    result.insert(QStringLiteral("justCompleted"), !newWinners.isEmpty());
    result.insert(QStringLiteral("newWinners"), newWinners);
    result.insert(QStringLiteral("winners"), winners);
    result.insert(QStringLiteral("scoreboard"), board);

    out.newWinners = newWinners;
    out.scoreboard = board;
    if (newChecked) {
        QVariantList outbound = groupedOverlays;
        if (!newWinners.isEmpty()) {
            QVariantMap winOv;
            winOv.insert(QStringLiteral("kind"), QStringLiteral("winner"));
            winOv.insert(QStringLiteral("winners"), newWinners);
            winOv.insert(QStringLiteral("scoreboard"), board);
            outbound.prepend(winOv);
        }
        out.outboundOverlays = outbound;
    }
    return out;
}

void resetAllPlayChecks(core::Project& project, store::Database* db)
{
    if (!db)
        return;
    const int rows = project.gridRows;
    const int cols = project.gridCols;
    for (const auto& g : project.grids) {
        const auto empty = checksToVariant(emptyChecks(rows, cols, core::projectHasFreeCenter(project)));
        db->savePlayChecks(project.id, g.player,
                           QJsonDocument(QJsonArray::fromVariantList(empty))
                               .toJson(QJsonDocument::Compact)
                               .toStdString());
    }
}

int computeScore(const core::Project& project, const QString& playerName,
                 const QVariantList& checks)
{
    const int rows = project.gridRows;
    const int cols = project.gridCols;
    for (const auto& grid : project.grids) {
        if (grid.player != playerName.toStdString())
            continue;
        return core::computeScore(grid, checksFromVariant(checks, rows, cols));
    }
    return 0;
}

QVariantList detectBingoLines(const core::Project& project, const QVariantList& checks)
{
    const int rows = project.gridRows;
    const int cols = project.gridCols;
    const auto lines = core::detectBingo(checksFromVariant(checks, rows, cols), rows, cols);
    QVariantList out;
    for (const auto& line : lines) {
        QVariantList coords;
        for (const auto& [r, c] : line)
            coords.append(QVariant(QVariantList{ r, c }));
        out.append(QVariant(coords));
    }
    return out;
}

QString winnersToastMessage(const QVariantList& newWinners)
{
    QStringList names;
    for (const QVariant& w : newWinners)
        names << w.toMap().value(QStringLiteral("player")).toString();
    names.removeAll(QString());
    if (names.isEmpty())
        return {};
    if (names.size() == 1)
        return names[0] + QStringLiteral(" a gagné — grille complète !");
    return names.join(QStringLiteral(", ")) + QStringLiteral(" ont gagné !");
}

} // namespace app::play

