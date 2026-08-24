#include "exportprint.h"
#include "playlogic.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QLinearGradient>
#include <QLocale>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPen>
#include <QTextOption>
#include <QtGlobal>

#include <limits>

namespace app::exportprint {


QImage renderScoreboardImage(const core::Project& project, store::Database* db)
{
    if (!db)
        return {};
    const QVariantList board = play::buildScoreboard(project, db);
    if (board.isEmpty())
        return {};

    const bool gageMode = project.gageMode;
    const QString unit = gageMode ? QStringLiteral("cases") : QStringLiteral("pts");
    QStringList winnerNames;
    for (const QVariant& rowV : board) {
        const auto m = rowV.toMap();
        if (m.value(QStringLiteral("full")).toBool())
            winnerNames << m.value(QStringLiteral("player")).toString();
    }

    const int W = 1000;
    const int pad = 40;

    auto fontPx = [](int px, bool bold = false) {
        QFont f = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
        f.setPixelSize(px);
        f.setBold(bold);
        return f;
    };
    auto elide = [](const QFont& f, const QString& t, int maxW) {
        return QFontMetrics(f).elidedText(t, Qt::ElideRight, maxW);
    };
    auto fmtScore = [&](int score) {
        return QLocale(QLocale::French).toString(score) + QLatin1Char(' ') + unit;
    };

    const QString title = QString::fromStdString(project.title).trimmed();
    const QString description = QString::fromStdString(project.description).trimmed();

    QFont titleFont = fontPx(32, true);
    QFont subFont = fontPx(14);
    QFont descFont = fontPx(15);

    const int titleTop = 24;
    const int titleH = 42;
    int cursorY = titleTop + titleH;

    QRect descRect;
    if (!description.isEmpty()) {
        cursorY += 4;
        const int descMaxW = W - 2 * pad;
        const QFontMetrics fm(descFont);
        const QRect bounds = fm.boundingRect(QRect(0, 0, descMaxW, 80),
                                             Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                                             description);
        const int descBlockH = qBound(20, bounds.height() + 4, 72);
        descRect = QRect(pad, cursorY, descMaxW, descBlockH);
        cursorY += descBlockH + 6;
    } else {
        cursorY += 4;
    }

    const int metaY = cursorY;
    const int metaH = 24;
    cursorY += metaH + 10;

    const int winnerBannerH = winnerNames.isEmpty() ? 0 : 72;
    const int headerH = cursorY + (winnerNames.isEmpty() ? 0 : winnerBannerH);
    const int podiumH = board.size() >= 1 ? 220 : 0;
    const int rowH = 70;
    const int footerH = 48;
    const int listStart = headerH + podiumH;
    const int H = listStart + board.size() * rowH + footerH;

    QImage img(W, H, QImage::Format_RGB32);
    img.fill(QColor(QStringLiteral("#0f1623")));

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // Accent top
    QLinearGradient bar(0, 0, W, 0);
    bar.setColorAt(0.0, QColor(QStringLiteral("#4f46e5")));
    bar.setColorAt(1.0, QColor(QStringLiteral("#22c55e")));
    p.fillRect(0, 0, W, 8, bar);

    p.setFont(titleFont);
    p.setPen(QColor(QStringLiteral("#f1f5f9")));
    p.drawText(QRect(pad, titleTop, W - 2 * pad, titleH), Qt::AlignLeft | Qt::AlignVCenter,
               elide(titleFont, title.isEmpty() ? QStringLiteral("Bingo") : title, W - 2 * pad));

    if (!description.isEmpty()) {
        p.setFont(descFont);
        p.setPen(QColor(QStringLiteral("#cbd5e1")));
        p.drawText(descRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, description);
    }

    p.setFont(subFont);
    p.setPen(QColor(QStringLiteral("#7c8fa6")));
    p.drawText(QRect(pad, metaY, W / 2, metaH), Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("Classement · Open Bingo"));
    p.drawText(QRect(W / 2, metaY, W / 2 - pad, metaH), Qt::AlignRight | Qt::AlignVCenter,
               QDateTime::currentDateTime().toString(QStringLiteral("dd/MM/yyyy  HH:mm")));

    int y = metaY + metaH + 10;
    if (!winnerNames.isEmpty()) {
        const QRectF banner(pad, y, W - 2 * pad, 56);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(34, 197, 94, 45));
        p.drawRoundedRect(banner, 14, 14);
        p.setPen(QPen(QColor(QStringLiteral("#22c55e")), 2));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(banner.adjusted(1, 1, -1, -1), 14, 14);

        QFont winFont = fontPx(18, true);
        p.setFont(winFont);
        p.setPen(QColor(QStringLiteral("#22c55e")));
        const QString winText = winnerNames.size() == 1
            ? (QStringLiteral("Gagnant : ") + winnerNames[0])
            : (QStringLiteral("Gagnants : ") + winnerNames.join(QStringLiteral(" · ")));
        p.drawText(banner.toRect().adjusted(18, 0, -18, 0), Qt::AlignVCenter | Qt::AlignLeft,
                   elide(winFont, winText, static_cast<int>(banner.width()) - 36));
        y += winnerBannerH;
    }

    // Podium top 3
    if (podiumH > 0 && board.size() >= 1) {
        const int podiumTop = y + 8;
        const int baseY = podiumTop + podiumH - 24;
        const int mid = W / 2;

        auto drawPodiumBlock = [&](int rankIdx, int cx, int blockH, const QColor& color) {
            if (rankIdx < 0 || rankIdx >= board.size())
                return;
            const auto m = board[rankIdx].toMap();
            const QString name = m.value(QStringLiteral("player")).toString();
            const int score = m.value(QStringLiteral("score")).toInt();
            const bool full = m.value(QStringLiteral("full")).toBool();
            const int bw = 170;
            const QRect block(cx - bw / 2, baseY - blockH, bw, blockH);
            p.setPen(Qt::NoPen);
            p.setBrush(color);
            p.drawRoundedRect(block, 12, 12);

            QFont rankF = fontPx(28, true);
            p.setFont(rankF);
            p.setPen(QColor(QStringLiteral("#0f1623")));
            p.drawText(QRect(block.x(), block.y() + 10, block.width(), 36), Qt::AlignCenter,
                       QString::number(rankIdx + 1));

            QFont nameF = fontPx(13, true);
            p.setFont(nameF);
            p.setPen(QColor(QStringLiteral("#0f1623")));
            p.drawText(QRect(block.x() + 8, block.y() + 48, block.width() - 16, 36),
                       Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap,
                       elide(nameF, name, block.width() - 16));

            QFont scoreF = fontPx(15, true);
            p.setFont(scoreF);
            p.setPen(full ? QColor(QStringLiteral("#14532d")) : QColor(QStringLiteral("#0f1623")));
            p.drawText(QRect(block.x() + 6, block.bottom() - 34, block.width() - 12, 28),
                       Qt::AlignCenter, elide(scoreF, fmtScore(score), block.width() - 12));
        };

        // Ordre visuel : 2 | 1 | 3
        if (board.size() >= 2)
            drawPodiumBlock(1, mid - 220, 110, QColor(QStringLiteral("#94a3b8")));
        drawPodiumBlock(0, mid, 150, QColor(QStringLiteral("#fbbf24")));
        if (board.size() >= 3)
            drawPodiumBlock(2, mid + 220, 90, QColor(QStringLiteral("#d97706")));
        y = listStart;
    } else {
        y = listStart;
    }

    // Liste complète
    for (int i = 0; i < board.size(); ++i) {
        const auto m = board[i].toMap();
        const QString name = m.value(QStringLiteral("player")).toString();
        const int score = m.value(QStringLiteral("score")).toInt();
        const int checked = m.value(QStringLiteral("checked")).toInt();
        const int total = qMax(1, m.value(QStringLiteral("total")).toInt());
        const bool full = m.value(QStringLiteral("full")).toBool();
        const int rank = i + 1;

        const QRectF rowRect(pad, y, W - 2 * pad, rowH - 10);
        p.setPen(Qt::NoPen);
        p.setBrush(full ? QColor(QStringLiteral("#1a2e24"))
                        : (i % 2 == 0 ? QColor(QStringLiteral("#1a2235"))
                                      : QColor(QStringLiteral("#151c2c"))));
        p.drawRoundedRect(rowRect, 12, 12);
        if (full) {
            p.setPen(QPen(QColor(QStringLiteral("#22c55e")), 2));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(rowRect.adjusted(1, 1, -1, -1), 12, 12);
        }

        QColor medal = QColor(QStringLiteral("#3d5270"));
        if (rank == 1) medal = QColor(QStringLiteral("#fbbf24"));
        else if (rank == 2) medal = QColor(QStringLiteral("#e2e8f0"));
        else if (rank == 3) medal = QColor(QStringLiteral("#d97706"));
        const QRectF medalRect(pad + 16, y + 14, 36, 36);
        p.setPen(Qt::NoPen);
        p.setBrush(medal);
        p.drawEllipse(medalRect);
        QFont rankFont = fontPx(14, true);
        p.setFont(rankFont);
        p.setPen(rank <= 3 ? QColor(QStringLiteral("#0f1623")) : QColor(QStringLiteral("#f1f5f9")));
        p.drawText(medalRect.toRect(), Qt::AlignCenter, QString::number(rank));

        const int nameX = pad + 66;
        const int scoreColW = 200;
        const int nameW = W - 2 * pad - 66 - scoreColW - 16;

        QFont nameFont = fontPx(full ? 17 : 16, full || rank <= 3);
        p.setFont(nameFont);
        p.setPen(QColor(QStringLiteral("#f1f5f9")));
        const QString nameDraw = full ? (name + QStringLiteral("  ·  gagnant")) : name;
        p.drawText(QRect(nameX, y + 6, nameW, 26), Qt::AlignLeft | Qt::AlignVCenter,
                   elide(nameFont, nameDraw, nameW));

        // Barre de progression (cases cochées / total jouables)
        const int barW = nameW;
        const int barX = nameX;
        const int barY = y + 40;
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(QStringLiteral("#0b1220")));
        p.drawRoundedRect(QRect(barX, barY, barW, 12), 6, 6);
        const int fillW = qBound(0, static_cast<int>(barW * (checked / static_cast<double>(total))), barW);
        p.setBrush(full ? QColor(QStringLiteral("#22c55e")) : QColor(QStringLiteral("#6366f1")));
        if (fillW > 0)
            p.drawRoundedRect(QRect(barX, barY, fillW, 12), 6, 6);

        QFont progFont = fontPx(11);
        p.setFont(progFont);
        p.setPen(QColor(QStringLiteral("#7c8fa6")));
        p.drawText(QRect(nameX, y + 54, nameW, 14), Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("%1 / %2").arg(checked).arg(total));

        QFont scoreFont = fontPx(20, true);
        p.setFont(scoreFont);
        p.setPen(full ? QColor(QStringLiteral("#22c55e")) : QColor(QStringLiteral("#a5b4fc")));
        p.drawText(QRect(W - pad - scoreColW - 8, y, scoreColW, rowH - 10),
                   Qt::AlignRight | Qt::AlignVCenter, fmtScore(score));

        y += rowH;
    }

    p.setPen(QColor(QStringLiteral("#5b6b82")));
    p.setFont(fontPx(12));
    p.drawText(QRect(pad, H - footerH, W - 2 * pad, footerH - 14), Qt::AlignCenter,
               QStringLiteral("Open Bingo  ·  %1 joueur%2%3")
                   .arg(board.size())
                   .arg(board.size() > 1 ? QStringLiteral("s") : QString())
                   .arg(winnerNames.isEmpty()
                            ? QString()
                            : QStringLiteral("  ·  %1 gagnant%2")
                                  .arg(winnerNames.size())
                                  .arg(winnerNames.size() > 1 ? QStringLiteral("s") : QString())));
    p.end();
    return img;
}

namespace {


qreal mmToPx(const QPrinter& printer, qreal mm)
{
    return mm * printer.resolution() / 25.4;
}

void drawPlayerSheet(QPainter& p, const QRectF& area, const core::Project& project,
                     const core::PlayerGrid& grid)
{
    const bool gageMode = project.gageMode;
    const bool hasGages = !project.gages.empty();
    qreal y = area.top();
    const qreal left = area.left();
    const qreal w = area.width();

    // En-tête : titre projet + nom joueur
    QFont titleFont(QStringLiteral("Sans Serif"));
    titleFont.setBold(true);
    titleFont.setPixelSize(qMax(10, int(area.height() * 0.032)));
    p.setFont(titleFont);
    p.setPen(Qt::black);
    p.drawText(QRectF(left, y, w, titleFont.pixelSize() * 1.35),
               Qt::AlignHCenter | Qt::AlignVCenter,
               QString::fromStdString(project.title));
    y += titleFont.pixelSize() * 1.4;

    QFont playerFont(titleFont);
    playerFont.setPixelSize(qMax(14, int(area.height() * 0.052)));
    playerFont.setBold(true);
    p.setFont(playerFont);
    p.drawText(QRectF(left, y, w, playerFont.pixelSize() * 1.25),
               Qt::AlignHCenter | Qt::AlignVCenter,
               QString::fromStdString(grid.player));
    y += playerFont.pixelSize() * 1.35;

    p.setPen(QPen(Qt::black, 2.2));
    p.drawLine(QPointF(left, y), QPointF(left + w, y));
    y += 8;

    // Pied réservé (HP / règles / note gage)
    const qreal footerH = area.height() * (gageMode ? 0.11 : 0.26);
    const qreal gridBottom = area.bottom() - footerH;
    QRectF gridArea(left, y, w, qMax(20.0, gridBottom - y));

    if (grid.cells.empty() || gridArea.height() < 20)
        return;

    const int rowsN = static_cast<int>(grid.cells.size());
    const int colsN = rowsN > 0
        ? static_cast<int>(grid.cells[0].size()) : 0;
    if (rowsN <= 0 || colsN <= 0)
        return;
    // Cases carrées centrées dans la zone réservée
    const qreal side = qMin(gridArea.width() / colsN, gridArea.height() / rowsN);
    const qreal gridW = side * colsN;
    const qreal gridH = side * rowsN;
    gridArea = QRectF(left + (w - gridW) / 2,
                      y + qMax(0.0, (gridArea.height() - gridH) / 2),
                      gridW, gridH);

    const int N = qMax(rowsN, colsN);
    const qreal fontPx = qMax(6.0, side * (N <= 3 ? 0.22 : N <= 5 ? 0.18 : 0.15));
    const qreal ptsPx = qMax(5.0, fontPx * 0.72);

    QFont cellFont(QStringLiteral("Sans Serif"));
    cellFont.setPixelSize(int(fontPx));
    QFont ptsFont(cellFont);
    ptsFont.setPixelSize(int(ptsPx));
    ptsFont.setBold(true);

    QTextOption opt;
    opt.setWrapMode(QTextOption::WordWrap);
    opt.setAlignment(Qt::AlignCenter);

    for (int r = 0; r < rowsN; ++r) {
        const auto& row = grid.cells[static_cast<size_t>(r)];
        for (int c = 0; c < colsN && c < static_cast<int>(row.size()); ++c) {
            const auto& cell = row[static_cast<size_t>(c)];
            const QRectF rect(gridArea.left() + c * side,
                              gridArea.top() + r * side,
                              side, side);
            if (cell.isFree)
                p.fillRect(rect, QColor(QStringLiteral("#e8e8e8")));
            else
                p.fillRect(rect, Qt::white);
            p.setPen(QPen(Qt::black, 2.0));
            p.drawRect(rect);

            const QString label = cell.isFree
                ? QStringLiteral("Libre")
                : QString::fromStdString(cell.label);
            p.setFont(cellFont);
            p.setPen(Qt::black);
            p.drawText(rect.adjusted(3, 2, -3, -ptsPx - 3), label, opt);

            if (!cell.isFree) {
                const QString pts = gageMode
                    ? (QStringLiteral("#") + QString::number(cell.points))
                    : QString::number(cell.points);
                p.setFont(ptsFont);
                p.setPen(gageMode ? QColor(QStringLiteral("#4f46e5"))
                                  : QColor(QStringLiteral("#555555")));
                p.drawText(rect.adjusted(2, 2, -4, -3),
                           Qt::AlignBottom | Qt::AlignRight, pts);
            }
        }
    }

    // Pied
    y = qMax(gridArea.bottom(), gridBottom - footerH) + 6;
    if (y > area.bottom() - 8)
        y = area.bottom() - footerH + 4;
    p.setPen(QPen(QColor(QStringLiteral("#bbbbbb")), 1));
    p.drawLine(QPointF(left, y), QPointF(left + w, y));
    y += 6;

    QFont small(QStringLiteral("Sans Serif"));
    small.setPixelSize(qMax(8, int(area.height() * 0.02)));
    QFont smallBold(small);
    smallBold.setBold(true);

    if (gageMode) {
        p.setFont(small);
        p.setPen(QColor(QStringLiteral("#333333")));
        QTextOption noteOpt;
        noteOpt.setWrapMode(QTextOption::WordWrap);
        p.drawText(QRectF(left, y, w, area.bottom() - y),
                   QStringLiteral("Le n° dans le coin bas-droit de chaque case est le "
                                  "numéro du gage à effectuer (voir la feuille « Tableau des Gages »)."),
                   noteOpt);
        return;
    }

    // Mode classique : cases PV + multiplicateurs (tableau 2×2 comme l'app web)
    p.setFont(smallBold);
    p.setPen(Qt::black);
    p.drawText(QRectF(left, y, w, smallBold.pixelSize() * 1.25),
               Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("Points de vie — %1 PV").arg(project.startHP));
    y += smallBold.pixelSize() * 1.4;

    const qreal box = qMax(9.0, small.pixelSize() * 1.15);
    const int hp = qBound(0, project.startHP, 80);
    qreal x = left;
    p.setPen(QPen(Qt::black, 1.4));
    for (int i = 0; i < hp; ++i) {
        if (x + box > left + w) {
            x = left;
            y += box + 3;
            if (y + box > area.bottom() - small.pixelSize() * 6)
                break;
        }
        p.drawRect(QRectF(x, y, box, box));
        x += box + 3;
    }
    y += box + 7;

    p.setFont(smallBold);
    p.drawText(QRectF(left, y, w, smallBold.pixelSize() * 1.25),
               Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("Règles des combinaisons"));
    y += smallBold.pixelSize() * 1.35;

    p.setFont(small);
    const qreal colW = w * 0.5;
    const qreal rowH = small.pixelSize() * 1.45;
    auto ruleRow = [&](const QString& a, const QString& av, const QString& b, const QString& bv) {
        if (y + rowH > area.bottom())
            return;
        p.drawText(QRectF(left, y, colW * 0.55, rowH), Qt::AlignVCenter | Qt::AlignLeft, a);
        p.setFont(smallBold);
        p.drawText(QRectF(left + colW * 0.55, y, colW * 0.45, rowH),
                   Qt::AlignVCenter | Qt::AlignLeft, av);
        p.setFont(small);
        p.drawText(QRectF(left + colW, y, colW * 0.55, rowH), Qt::AlignVCenter | Qt::AlignLeft, b);
        p.setFont(smallBold);
        p.drawText(QRectF(left + colW + colW * 0.55, y, colW * 0.45, rowH),
                   Qt::AlignVCenter | Qt::AlignLeft, bv);
        p.setFont(small);
        y += rowH;
    };
    ruleRow(QStringLiteral("Ligne complète"),
            QStringLiteral("× %1").arg(project.multipliers.line),
            QStringLiteral("Colonne complète"),
            QStringLiteral("× %1").arg(project.multipliers.column));
    ruleRow(QStringLiteral("Diagonale complète"),
            QStringLiteral("× %1").arg(project.multipliers.diagonal),
            QStringLiteral("Grille complète (BINGO)"),
            QStringLiteral("× %1").arg(project.multipliers.full));

    y += 3;
    p.setFont(small);
    p.setPen(QColor(QStringLiteral("#444444")));
    QTextOption rulesOpt;
    rulesOpt.setWrapMode(QTextOption::WordWrap);
    const QString note = QStringLiteral(
        "Coche une case quand l'événement se produit. La valeur en points est "
        "dans le coin bas-droit.%1")
        .arg(hasGages
                 ? QStringLiteral(" Pour récupérer des PV, accomplis un gage "
                                  "(feuille « Tableau des Gages »).")
                 : QString());
    p.drawText(QRectF(left, y, w, area.bottom() - y), note, rulesOpt);
}

// Estimation de la taille de case (px) si on dessine la grille dans `sheetArea`.
qreal estimateCellSide(const QRectF& sheetArea, const core::Project& project,
                       const core::PlayerGrid& grid)
{
    if (grid.cells.empty())
        return 0;
    const int rowsN = static_cast<int>(grid.cells.size());
    const int colsN = rowsN > 0 ? static_cast<int>(grid.cells[0].size()) : 0;
    if (rowsN <= 0 || colsN <= 0)
        return 0;

    const bool gageMode = project.gageMode;
    // Même proportions que drawPlayerSheet (en-tête + pied).
    const qreal headerH = sheetArea.height() * 0.12;
    const qreal footerH = sheetArea.height() * (gageMode ? 0.11 : 0.26);
    const qreal gridH = qMax(1.0, sheetArea.height() - headerH - footerH);
    const qreal gridW = sheetArea.width();
    return qMin(gridW / colsN, gridH / rowsN);
}

// true = 2 grilles / page (demi A4) si les cases restent assez grandes ; sinon 1 / page.
bool preferHalfPageLayout(const QPrinter& printer, const QRectF& page,
                          const core::Project& project)
{
    if (project.grids.empty())
        return true;

    const qreal cutBand = mmToPx(printer, 5);
    const qreal halfH = (page.height() - cutBand) / 2.0;
    const qreal insetX = mmToPx(printer, 4);
    const qreal insetY = mmToPx(printer, 2);
    const QRectF halfArea(page.left() + insetX,
                          page.top() + insetY,
                          page.width() - 2 * insetX,
                          halfH - 2 * insetY);

    // Seuil lisible ≈ 18 mm de côté de case (texte + n° / points).
    const qreal minSide = mmToPx(printer, 18);

    qreal worst = std::numeric_limits<qreal>::max();
    for (const auto& g : project.grids)
        worst = qMin(worst, estimateCellSide(halfArea, project, g));
    return worst >= minSide;
}

// Dessine une page (ou suite) du tableau des gages.
// nextGage : prochain index dans project.gages
// nextCombo : 0=titre section, 1=ligne, 2=colonne, 3=diagonale, 4=terminé
// Retourne false s'il reste du contenu (il faut newPage).
bool drawGageSheetPage(QPainter& p, const QRectF& area, const core::Project& project,
                       size_t& nextGage, int& nextCombo, bool firstPage)
{
    qreal y = area.top();
    const qreal left = area.left();
    const qreal w = area.width();
    const qreal bottom = area.bottom();

    QFont titleFont(QStringLiteral("Sans Serif"));
    titleFont.setBold(true);
    titleFont.setPixelSize(qMax(10, int(area.height() * 0.028)));
    p.setFont(titleFont);
    p.setPen(Qt::black);
    p.drawText(QRectF(left, y, w, titleFont.pixelSize() * 1.4),
               Qt::AlignHCenter | Qt::AlignVCenter,
               QString::fromStdString(project.title));
    y += titleFont.pixelSize() * 1.45;

    QFont playerFont(titleFont);
    playerFont.setPixelSize(qMax(13, int(area.height() * 0.04)));
    p.setFont(playerFont);
    p.drawText(QRectF(left, y, w, playerFont.pixelSize() * 1.3),
               Qt::AlignHCenter | Qt::AlignVCenter,
               firstPage ? QStringLiteral("Tableau des Gages")
                         : QStringLiteral("Tableau des Gages (suite)"));
    y += playerFont.pixelSize() * 1.4;

    p.setPen(QPen(Qt::black, 2));
    p.drawLine(QPointF(left, y), QPointF(left + w, y));
    y += 8;

    QFont body(QStringLiteral("Sans Serif"));
    body.setPixelSize(qMax(9, int(area.height() * 0.022)));
    QFontMetrics fm(body);

    if (firstPage) {
        QFont bodyItalic(body);
        bodyItalic.setItalic(true);
        p.setFont(bodyItalic);
        p.setPen(QColor(QStringLiteral("#333333")));
        const QString intro = project.gageMode
            ? QStringLiteral("Le n° sur chaque case tire un gage parmi ceux qui portent "
                             "ce numéro (selon le %). Effectue-le quand tu tombes dessus !")
            : QStringLiteral("Accomplis n'importe quel gage pour récupérer des points "
                             "de vie. Une fois accompli, coche-le.");
        const QRect introBound = fm.boundingRect(QRect(0, 0, int(w), 1000),
                                                Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                                                intro);
        const qreal introH = qMax(qreal(body.pixelSize() * 2.4), qreal(introBound.height() + 4));
        QTextOption introOpt;
        introOpt.setWrapMode(QTextOption::WordWrap);
        p.setFont(bodyItalic);
        p.drawText(QRectF(left, y, w, introH), intro, introOpt);
        y += introH + 6;
    }

    const bool showHp = !project.gageMode;
    const bool showRate = project.gageMode;
    const qreal colNum = w * 0.10;
    const qreal colRate = showRate ? w * 0.12 : 0;
    const qreal colHp = showHp ? w * 0.16 : 0;
    const qreal colDesc = w - colNum - colRate - colHp;
    const qreal minRowH = qMax(18.0, body.pixelSize() * 1.8);
    const qreal descPad = 10.0;

    auto drawHeaderRow = [&]() {
        auto drawHeaderCell = [&](const QRectF& r, const QString& text) {
            p.fillRect(r, QColor(QStringLiteral("#f0f0f0")));
            p.setPen(QPen(Qt::black, 1.2));
            p.drawRect(r);
            QFont h = body;
            h.setBold(true);
            p.setFont(h);
            p.setPen(Qt::black);
            p.drawText(r.adjusted(4, 0, -4, 0), Qt::AlignVCenter | Qt::AlignLeft, text);
        };
        if (y + minRowH > bottom)
            return false;
        drawHeaderCell(QRectF(left, y, colNum, minRowH), QStringLiteral("#"));
        drawHeaderCell(QRectF(left + colNum, y, colDesc, minRowH), QStringLiteral("Gage"));
        if (showRate)
            drawHeaderCell(QRectF(left + colNum + colDesc, y, colRate, minRowH),
                           QStringLiteral("%"));
        if (showHp)
            drawHeaderCell(QRectF(left + colNum + colDesc + colRate, y, colHp, minRowH),
                           QStringLiteral("PV"));
        y += minRowH;
        return true;
    };

    // En-tête de tableau tant qu'il reste des gages à peindre.
    if (nextGage < project.gages.size()) {
        if (!drawHeaderRow())
            return false;
    }

    const qreal bodyStartY = y;
    p.setFont(body);
    p.setPen(Qt::black);
    while (nextGage < project.gages.size()) {
        const auto& g = project.gages[nextGage];
        const QString desc = QString::fromStdString(g.description);
        const QRect descBound = fm.boundingRect(
            QRect(0, 0, qMax(1, int(colDesc - descPad)), 4000),
            Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, desc);
        qreal rowH = qMax(minRowH, qreal(descBound.height()) + 8);

        const qreal room = bottom - y;
        if (rowH > room) {
            // Page suivante sauf si c'est la 1ʳᵉ ligne du corps (description > page).
            if (y <= bodyStartY + 0.5 && room >= minRowH)
                rowH = room;
            else
                return false;
        }

        const QRectF rNum(left, y, colNum, rowH);
        const QRectF rDesc(left + colNum, y, colDesc, rowH);
        const QRectF rRate(left + colNum + colDesc, y, colRate, rowH);
        const QRectF rHp(left + colNum + colDesc + colRate, y, colHp, rowH);
        p.setPen(QPen(Qt::black, 1));
        p.drawRect(rNum);
        p.drawRect(rDesc);
        if (showRate)
            p.drawRect(rRate);
        if (showHp)
            p.drawRect(rHp);
        p.setFont(body);
        p.setPen(Qt::black);
        p.drawText(rNum, Qt::AlignCenter, QString::number(g.number));
        p.drawText(rDesc.adjusted(6, 4, -4, -4),
                   Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, desc);
        if (showRate)
            p.drawText(rRate, Qt::AlignCenter, QString::number(g.rate) + QLatin1Char('%'));
        if (showHp)
            p.drawText(rHp, Qt::AlignCenter, QStringLiteral("+%1 PV").arg(g.hp));
        y += rowH;
        ++nextGage;
    }

    const auto& combos = project.comboGages;
    const bool hasCombos = project.gageMode
        && (!combos.line.empty() || !combos.column.empty() || !combos.diagonal.empty());
    if (!hasCombos) {
        nextCombo = 4;
        return true;
    }

    struct ComboItem {
        const char* label;
        const std::string* text;
    };
    const ComboItem items[] = {
        { "Ligne complète", &combos.line },
        { "Colonne complète", &combos.column },
        { "Diagonale complète", &combos.diagonal },
    };

    if (nextCombo == 0) {
        y += 12;
        QFont comboTitle(body);
        comboTitle.setBold(true);
        const qreal th = comboTitle.pixelSize() * 1.5;
        if (y + th > bottom)
            return false;
        p.setFont(comboTitle);
        p.setPen(Qt::black);
        p.drawText(QRectF(left, y, w, th), Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("Gages de combinaison"));
        y += th + 4;
        nextCombo = 1;
    }

    while (nextCombo >= 1 && nextCombo <= 3) {
        const ComboItem& it = items[nextCombo - 1];
        if (it.text->empty()) {
            ++nextCombo;
            continue;
        }
        const QString text = QString::fromStdString(*it.text);
        const qreal typeW = w * 0.28;
        const qreal textW = w * 0.72;
        const QRect textBound = fm.boundingRect(
            QRect(0, 0, qMax(1, int(textW - descPad)), 4000),
            Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, text);
        qreal rowH = qMax(minRowH, qreal(textBound.height()) + 8);
        const qreal room = bottom - y;
        if (rowH > room) {
            if (room >= minRowH * 1.2)
                rowH = room;
            else
                return false;
        }
        const QRectF rType(left, y, typeW, rowH);
        const QRectF rText(left + typeW, y, textW, rowH);
        p.setPen(QPen(Qt::black, 1));
        p.drawRect(rType);
        p.drawRect(rText);
        QFont bold = body;
        bold.setBold(true);
        p.setFont(bold);
        p.setPen(Qt::black);
        p.drawText(rType.adjusted(4, 2, -4, -2),
                   Qt::AlignVCenter | Qt::AlignLeft | Qt::TextWordWrap,
                   QString::fromUtf8(it.label));
        p.setFont(body);
        p.drawText(rText.adjusted(6, 4, -4, -4),
                   Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, text);
        y += rowH;
        ++nextCombo;
    }

    nextCombo = 4;
    return true;
}

bool hasGagePages(const core::Project& project)
{
    if (!project.gages.empty())
        return true;
    if (!project.gageMode)
        return false;
    const auto& c = project.comboGages;
    return !c.line.empty() || !c.column.empty() || !c.diagonal.empty();
}

} // namespace

void paintBingoDocument(QPrinter& printer, const core::Project& project)
{
    QPainter painter;
    if (!painter.begin(&printer))
        return;

    const QRectF page = printer.pageRect(QPrinter::DevicePixel);
    const qreal insetX = mmToPx(printer, 4);
    const qreal insetY = mmToPx(printer, 2);
    const bool halfPage = preferHalfPageLayout(printer, page, project);
    const int perPage = halfPage ? 2 : 1;
    const qreal cutBand = halfPage ? mmToPx(printer, 5) : 0;
    const qreal slotH = halfPage ? (page.height() - cutBand) / 2.0 : page.height();

    auto drawCutGuide = [&](qreal midY) {
        QPen dash(QColor(QStringLiteral("#888888")), 1.2, Qt::DashLine);
        dash.setDashPattern({ 4, 3 });
        painter.setPen(dash);
        painter.drawLine(QPointF(page.left(), midY), QPointF(page.right(), midY));

        QFont mark(QStringLiteral("Sans Serif"));
        mark.setPixelSize(qMax(8, int(mmToPx(printer, 2.8))));
        painter.setFont(mark);
        painter.setPen(QColor(QStringLiteral("#666666")));
        const QString scissors = QStringLiteral("✂ découper");
        painter.drawText(QRectF(page.left(), midY - mark.pixelSize() * 0.7,
                                page.width() * 0.45, mark.pixelSize() * 1.4),
                         Qt::AlignLeft | Qt::AlignVCenter, scissors);
        painter.drawText(QRectF(page.left() + page.width() * 0.55,
                                midY - mark.pixelSize() * 0.7,
                                page.width() * 0.45, mark.pixelSize() * 1.4),
                         Qt::AlignRight | Qt::AlignVCenter, scissors);
    };

    int slot = 0;
    for (size_t i = 0; i < project.grids.size(); ++i) {
        if (slot == perPage) {
            if (!printer.newPage()) {
                painter.end();
                return;
            }
            slot = 0;
        }
        if (halfPage && slot == 1)
            drawCutGuide(page.top() + slotH + cutBand * 0.5);

        const qreal top = page.top() + slot * (slotH + cutBand);
        const QRectF area(page.left() + insetX,
                          top + insetY,
                          page.width() - 2 * insetX,
                          slotH - 2 * insetY);
        drawPlayerSheet(painter, area, project, project.grids[i]);
        ++slot;
    }

    if (hasGagePages(project)) {
        size_t nextGage = 0;
        int nextCombo = 0;
        bool firstGagePage = true;
        for (;;) {
            if (!printer.newPage()) {
                painter.end();
                return;
            }
            const QRectF gageArea(page.left() + insetX,
                                  page.top() + insetY,
                                  page.width() - 2 * insetX,
                                  page.height() - 2 * insetY);
            const size_t beforeGage = nextGage;
            const int beforeCombo = nextCombo;
            if (drawGageSheetPage(painter, gageArea, project, nextGage, nextCombo,
                                  firstGagePage))
                break;
            // Garde-fou : si rien n'a avancé (feuille trop petite), on saute un item.
            if (nextGage == beforeGage && nextCombo == beforeCombo) {
                if (nextGage < project.gages.size())
                    ++nextGage;
                else if (nextCombo < 4)
                    ++nextCombo;
                else
                    break;
            }
            firstGagePage = false;
        }
    }

    painter.end();
}



bool writePdf(const core::Project& project, const QString& filePath)
{
    if (project.grids.empty() || filePath.isEmpty())
        return false;

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(filePath);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageOrientation(QPageLayout::Portrait);
    printer.setPageMargins(QMarginsF(8, 6, 8, 6), QPageLayout::Millimeter);

    paintBingoDocument(printer, project);
    return QFile::exists(filePath) && QFileInfo(filePath).size() > 500;
}


} // namespace app::exportprint
