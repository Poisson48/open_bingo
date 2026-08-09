#include "theme.h"

#include <QDebug>

namespace app {

Theme::Theme(QObject *parent)
    : QObject(parent)
{
    // Identité sombre uniquement : pas de palette claire (évite un faux « light »).
    const QByteArray forced = qgetenv("COLO_THEME");
    if (forced == "light")
        qWarning("COLO_THEME=light ignoré — Open Bingo est dark-only");
    m_dark = true;
}

} // namespace app
