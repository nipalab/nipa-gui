#include "locksmodel.h"

#include <QFont>

LocksModel::LocksModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

void LocksModel::setLocks(const QList<FileLockInfo> &locks)
{
    beginResetModel();
    locks_ = locks;
    endResetModel();
}

void LocksModel::clear()
{
    beginResetModel();
    locks_.clear();
    endResetModel();
}

QList<FileLockInfo> LocksModel::locks() const
{
    return locks_;
}

FileLockInfo LocksModel::lockAt(int row) const
{
    if (row < 0 || row >= locks_.size()) {
        return {};
    }
    return locks_.at(row);
}

QString LocksModel::pathAt(int row) const
{
    if (row < 0 || row >= locks_.size()) {
        return {};
    }
    return locks_.at(row).path;
}

QString LocksModel::scopeLabel(const FileLockInfo &lock)
{
    if (lock.global || lock.branch.isEmpty()) {
        return tr("Mainline");
    }
    return lock.branch;
}

int LocksModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return locks_.size();
}

int LocksModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return ColumnCount;
}

QVariant LocksModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= locks_.size()) {
        return {};
    }
    const FileLockInfo &lock = locks_.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case PathColumn:
            return lock.path;
        case ScopeColumn:
            return scopeLabel(lock);
        case HolderColumn:
            return lock.heldByName.isEmpty() ? lock.heldBy : lock.heldByName;
        case AcquiredColumn:
            return lock.acquiredAt.toLocalTime().toString(QStringLiteral("yyyy-MM-dd hh:mm"));
        default:
            return {};
        }
    case Qt::ToolTipRole: {
        QString tip = tr("%1\nScope: %2\nHeld by: %3").arg(lock.path, scopeLabel(lock),
                                                           lock.heldByName.isEmpty() ? lock.heldBy
                                                                                     : lock.heldByName);
        if (lock.hasMergeRequest) {
            tip += tr("\nMerge request #%1").arg(lock.mergeRequestNumber);
        }
        return tip;
    }
    case Qt::FontRole:
        if (index.column() == PathColumn) {
            QFont font;
            font.setFamilies({QStringLiteral("monospace")});
            return font;
        }
        return {};
    default:
        return {};
    }
}

QVariant LocksModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal) {
        return {};
    }
    switch (section) {
    case PathColumn:
        return tr("Path");
    case ScopeColumn:
        return tr("Scope");
    case HolderColumn:
        return tr("Held by");
    case AcquiredColumn:
        return tr("Acquired");
    default:
        return {};
    }
}
