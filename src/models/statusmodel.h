#pragma once

#include <QAbstractTableModel>
#include <QHash>
#include <QList>

#include "daemon/daemontypes.h"

/// Table model over a StatusSnapshot: one row per changed file with a P4V-style
/// single-character badge (A/M/?/!...). Category filtering happens in the model
/// so the dock always shows exactly one pending list, like P4V's changelists.
class StatusModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column {
        StatusColumn = 0,
        PathColumn = 1,
        ColumnCount = 2,
    };
    Q_ENUM(Column)

    /// Ordered by display priority: conflicts and missing files surface first.
    enum Category {
        Conflicts = 0,
        Missing,
        Deleted,
        Modified,
        Staged,
        Untracked,
        CategoryCount,
    };
    Q_ENUM(Category)

    enum Role {
        CategoryRole = Qt::UserRole + 1,
    };

    explicit StatusModel(QObject *parent = nullptr);

    void setSnapshot(const StatusSnapshot &snapshot);
    void clear();
    const StatusSnapshot &snapshot() const;

    int count(Category category) const;

    /// -1 shows every category.
    void setCategoryFilter(int category);
    int categoryFilter() const;

    static QString categoryLabel(Category category);
    static QString categoryBadge(Category category);
    static QColor categoryColor(Category category);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    struct Row {
        Category category;
        QString path;
    };

    void rebuild();

    StatusSnapshot snapshot_;
    QList<Row> allRows_;
    QList<Row> rows_;
    QHash<int, int> counts_;
    int filter_ = -1;
};
