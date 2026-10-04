#include "branchmodel.h"

#include <QFont>

BranchModel::BranchModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

void BranchModel::setBranches(const QList<BranchInfo> &branches)
{
    beginResetModel();
    branches_ = branches;
    endResetModel();
}

void BranchModel::clear()
{
    beginResetModel();
    branches_.clear();
    endResetModel();
}

QList<BranchInfo> BranchModel::branches() const
{
    return branches_;
}

BranchInfo BranchModel::branchAt(int row) const
{
    if (row < 0 || row >= branches_.size()) {
        return {};
    }
    return branches_.at(row);
}

QString BranchModel::nameAt(int row) const
{
    return branchAt(row).name;
}

int BranchModel::rowForName(const QString &name) const
{
    for (int row = 0; row < branches_.size(); ++row) {
        if (branches_.at(row).name == name) {
            return row;
        }
    }
    return -1;
}

int BranchModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return branches_.size();
}

int BranchModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return ColumnCount;
}

QVariant BranchModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= branches_.size()) {
        return {};
    }
    const BranchInfo &branch = branches_.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case NameColumn:
            return branch.name;
        case HeadColumn:
            return branch.commitId.left(8);
        case DefaultColumn:
            return branch.isDefault ? QStringLiteral("✓") : QString();
        case ProtectedColumn:
            return branch.isProtected ? QStringLiteral("✓") : QString();
        default:
            return {};
        }
    case Qt::ToolTipRole:
        return tr("%1\nhead: %2\ncreated: %3")
            .arg(branch.name, branch.commitId.isEmpty() ? tr("(no commits)") : branch.commitId,
                 branch.createdAt.toLocalTime().toString(Qt::ISODate));
    case Qt::TextAlignmentRole:
        if (index.column() == DefaultColumn || index.column() == ProtectedColumn) {
            return QVariant::fromValue(Qt::AlignCenter);
        }
        return QVariant::fromValue(Qt::AlignLeft | Qt::AlignVCenter);
    case Qt::FontRole:
        if (index.column() == HeadColumn) {
            QFont font;
            font.setFamilies({QStringLiteral("monospace")});
            return font;
        }
        return {};
    default:
        return {};
    }
}

QVariant BranchModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal) {
        return {};
    }
    switch (section) {
    case NameColumn:
        return tr("Branch");
    case HeadColumn:
        return tr("Head");
    case DefaultColumn:
        return tr("Default");
    case ProtectedColumn:
        return tr("Protected");
    default:
        return {};
    }
}
