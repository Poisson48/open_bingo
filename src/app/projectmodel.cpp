#include "projectmodel.h"

#include <QColor>

namespace app {

ProjectModel::ProjectModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int ProjectModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(m_projects.size());
}

QVariant ProjectModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= static_cast<int>(m_projects.size()))
        return {};

    const auto& p = m_projects[static_cast<size_t>(index.row())];
    switch (role) {
    case IdRole: return QString::fromStdString(p.id);
    case TitleRole: return QString::fromStdString(p.title);
    case DescriptionRole: return QString::fromStdString(p.description);
    case UpdatedAtRole: return static_cast<qlonglong>(p.updatedAt);
    case GridSizeRole: return p.gridSize;
    case PlayerCountRole: return static_cast<int>(p.players.size());
    case CaseCountRole: return static_cast<int>(p.cases.size());
    case GridCountRole: return static_cast<int>(p.grids.size());
    case AccentRole: return accentForId(QString::fromStdString(p.id));
    default: return {};
    }
}

QHash<int, QByteArray> ProjectModel::roleNames() const
{
    return {
        { IdRole, "projectId" },
        { TitleRole, "title" },
        { DescriptionRole, "description" },
        { UpdatedAtRole, "updatedAt" },
        { GridSizeRole, "gridSize" },
        { PlayerCountRole, "playerCount" },
        { CaseCountRole, "caseCount" },
        { GridCountRole, "gridCount" },
        { AccentRole, "accent" },
    };
}

void ProjectModel::setProjects(std::vector<core::Project> projects)
{
    beginResetModel();
    m_projects = std::move(projects);
    endResetModel();
    emit countChanged();
}

QString ProjectModel::idAt(int row) const
{
    if (row < 0 || row >= static_cast<int>(m_projects.size()))
        return {};
    return QString::fromStdString(m_projects[static_cast<size_t>(row)].id);
}

QString ProjectModel::accentForId(const QString& id)
{
    uint hash = 0;
    for (const QChar ch : id)
        hash = hash * 31 + ch.unicode();
    const int hue = static_cast<int>(hash % 360);
    return QColor::fromHsv(hue, 140, 200).name(QColor::HexRgb);
}

} // namespace app
