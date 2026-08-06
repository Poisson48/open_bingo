#pragma once

#include "../core/bingotypes.h"

#include <QAbstractListModel>
#include <QHash>
#include <QVariant>

namespace app {

class ProjectModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        DescriptionRole,
        UpdatedAtRole,
        GridSizeRole,
        PlayerCountRole,
        CaseCountRole,
        GridCountRole,
        AccentRole,
    };

    explicit ProjectModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setProjects(std::vector<core::Project> projects);
    QString idAt(int row) const;

private:
    static QString accentForId(const QString& id);

    std::vector<core::Project> m_projects;
};

} // namespace app
