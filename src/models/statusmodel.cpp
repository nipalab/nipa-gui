#include "statusmodel.h"

#include <QBrush>
#include <QColor>
#include <QFont>

#include <algorithm>

StatusModel::StatusModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

void StatusModel::setSnapshot(const StatusSnapshot &snapshot)
{
    snapshot_ = snapshot;
    rebuild();
}

void StatusModel::clear()
{
    snapshot_ = {};
    rebuild();
}

const StatusSnapshot &StatusModel::snapshot() const
{
    return snapshot_;
}

int StatusModel::count(Category category) const
{
    return counts_.value(static_cast<int>(category), 0);
}

void StatusModel::setCategoryFilter(int category)
{
    if (filter_ == category) {
        return;
    }
    filter_ = category;
    beginResetModel();
    if (filter_ < 0) {
        rows_ = allRows_;
    } else {
        rows_.clear();
        for (const Row &row : allRows_) {
            if (static_cast<int>(row.category) == filter_) {
                rows_.append(row);
            }
        }
    }
    endResetModel();
}

int StatusModel::categoryFilter() const
{
    return filter_;
}

QString StatusModel::categoryLabel(Category category)
{
    switch (category) {
    case Staged:
        return tr("Staged");
    case Deleted:
        return tr("Deleted");
    case Modified:
        return tr("Modified");
    case Untracked:
        return tr("Untracked");
    case Missing:
        return tr("Missing");
    case Conflicts:
        return tr("Conflicts");
    case CategoryCount:
        break;
    }
    return {};
}

QString StatusModel::categoryBadge(Category category)
{
    switch (category) {
    case Staged:
        return QStringLiteral("A");
    case Deleted:
        return QStringLiteral("D");
    case Modified:
        return QStringLiteral("M");
    case Untracked:
        return QStringLiteral("?");
    case Missing:
        return QStringLiteral("!");
    case Conflicts:
        return QStringLiteral("C");
    case CategoryCount:
        break;
    }
    return {};
}

QColor StatusModel::categoryColor(Category category)
{
    switch (category) {
    case Staged:
        return QColor(0x1a, 0x7f, 0x37);
    case Deleted:
        return QColor(0xcf, 0x22, 0x2e);
    case Modified:
        return QColor(0x09, 0x69, 0xda);
    case Untracked:
        return QColor(0x6e, 0x77, 0x81);
    case Missing:
        return QColor(0x9a, 0x67, 0x00);
    case Conflicts:
        return QColor(0xbc, 0x4c, 0x00);
    case CategoryCount:
        break;
    }
    return {};
}

int StatusModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return rows_.size();
}

int StatusModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return ColumnCount;
}

QVariant StatusModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rows_.size()) {
        return {};
    }
    const Row &row = rows_.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
        if (index.column() == StatusColumn) {
            return categoryBadge(row.category);
        }
        if (index.column() == PathColumn) {
            return row.path;
        }
        return {};
    case Qt::ToolTipRole:
        return tr("%1 (%2)").arg(row.path, categoryLabel(row.category));
    case Qt::TextAlignmentRole:
        if (index.column() == StatusColumn) {
            return QVariant::fromValue(Qt::AlignCenter);
        }
        return QVariant::fromValue(Qt::AlignLeft | Qt::AlignVCenter);
    case Qt::ForegroundRole:
        if (index.column() == StatusColumn) {
            return QBrush(categoryColor(row.category));
        }
        return {};
    case Qt::FontRole:
        if (index.column() == StatusColumn) {
            QFont font;
            font.setBold(true);
            return font;
        }
        return {};
    case CategoryRole:
        return static_cast<int>(row.category);
    default:
        return {};
    }
}

QVariant StatusModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal) {
        return {};
    }
    switch (section) {
    case StatusColumn:
        return tr("Status");
    case PathColumn:
        return tr("File");
    default:
        return {};
    }
}

void StatusModel::rebuild()
{
    beginResetModel();

    allRows_.clear();
    counts_.clear();
    for (int category = 0; category < CategoryCount; ++category) {
        counts_.insert(category, 0);
    }

    const auto append = [this](Category category, const QStringList &paths) {
        counts_[static_cast<int>(category)] = paths.size();
        for (const QString &path : paths) {
            allRows_.append(Row{category, path});
        }
    };

    append(Conflicts, snapshot_.conflicts);
    append(Missing, snapshot_.missing);
    append(Deleted, snapshot_.deleted);
    append(Modified, snapshot_.modified);
    append(Staged, snapshot_.staged);
    append(Untracked, snapshot_.untracked);

    std::stable_sort(allRows_.begin(), allRows_.end(), [](const Row &a, const Row &b) {
        if (a.category != b.category) {
            return a.category < b.category;
        }
        return a.path < b.path;
    });

    rows_.clear();
    if (filter_ < 0) {
        rows_ = allRows_;
    } else {
        for (const Row &row : allRows_) {
            if (static_cast<int>(row.category) == filter_) {
                rows_.append(row);
            }
        }
    }

    endResetModel();
}
