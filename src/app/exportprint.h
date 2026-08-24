#pragma once

#include "../core/bingotypes.h"
#include "../store/database.h"

#include <QImage>
#include <QPrinter>
#include <QString>

namespace app::exportprint {

// Classement PNG (podium + liste) à partir du scoreboard play.
QImage renderScoreboardImage(const core::Project& project, store::Database* db);

// Peint grilles (+ feuille gages) sur une imprimante / PDF.
void paintBingoDocument(QPrinter& printer, const core::Project& project);

// Écrit un PDF A4 ; false si chemin vide, pas de grilles, ou fichier trop petit.
bool writePdf(const core::Project& project, const QString& filePath);

} // namespace app::exportprint
