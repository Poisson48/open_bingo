#pragma once

#include "../core/bingotypes.h"

#include <QAbstractListModel>
#include <QHash>
#include <QSet>
#include <QVariant>

namespace app {

class ProjectModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

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
        SharedRole,
    };

    explicit ProjectModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setProjects(std::vector<core::Project> projects);
    void setSharedIds(const QSet<QString>& sharedIds);
    QString idAt(int row) const;

signals:
    void countChanged();

private:
    static QString accentForId(const QString& id);

    std::vector<core::Project> m_projects;
    QSet<QString> m_sharedIds;
};

} // namespace app
