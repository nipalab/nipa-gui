#include "mergerequestmodel.h"

#include <QBrush>
#include <QColor>
#include <QFont>

MergeRequestModel::MergeRequestModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

void MergeRequestModel::setMergeRequests(const QList<MergeRequestInfo> &mergeRequests)
{
    beginResetModel();
    mergeRequests_ = mergeRequests;
    endResetModel();
}

void MergeRequestModel::clear()
{
    beginResetModel();
    mergeRequests_.clear();
    endResetModel();
}

QList<MergeRequestInfo> MergeRequestModel::mergeRequests() const
{
    return mergeRequests_;
}

MergeRequestInfo MergeRequestModel::mergeRequestAt(int row) const
{
    if (row < 0 || row >= mergeRequests_.size()) {
        return {};
    }
    return mergeRequests_.at(row);
}

qint64 MergeRequestModel::numberAt(int row) const
{
    return mergeRequestAt(row).number;
}

int MergeRequestModel::rowForNumber(qint64 number) const
{
    for (int row = 0; row < mergeRequests_.size(); ++row) {
        if (mergeRequests_.at(row).number == number) {
            return row;
        }
    }
    return -1;
}

int MergeRequestModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return mergeRequests_.size();
}

int MergeRequestModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return ColumnCount;
}

QVariant MergeRequestModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= mergeRequests_.size()) {
        return {};
    }
    const MergeRequestInfo &mergeRequest = mergeRequests_.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case NumberColumn:
            return QStringLiteral("#%1").arg(mergeRequest.number);
        case TitleColumn:
            return mergeRequest.title;
        case SourceColumn:
            return mergeRequest.sourceBranch;
        case TargetColumn:
            return mergeRequest.targetBranch;
        case StatusColumn:
            return mergeRequest.status;
        case UpdatedColumn:
            return mergeRequest.updatedAt.toLocalTime().toString(QStringLiteral("yyyy-MM-dd hh:mm"));
        default:
            return {};
        }
    case Qt::ToolTipRole:
        return tr("#%1 %2\n%3 → %4\ncreated by: %5\n\n%6")
            .arg(mergeRequest.number)
            .arg(mergeRequest.title, mergeRequest.sourceBranch, mergeRequest.targetBranch,
                 mergeRequest.createdBy, mergeRequest.description);
    case Qt::ForegroundRole:
        if (index.column() == StatusColumn) {
            if (mergeRequest.status == QLatin1String("open")) {
                return QBrush(QColor(0x1a, 0x7f, 0x37));
            }
            if (mergeRequest.status == QLatin1String("merged")) {
                return QBrush(QColor(0x82, 0x50, 0xdf));
            }
            return QBrush(QColor(0x6e, 0x77, 0x81));
        }
        return {};
    case Qt::FontRole:
        if (index.column() == NumberColumn) {
            QFont font;
            font.setBold(true);
            return font;
        }
        return {};
    default:
        return {};
    }
}

QVariant MergeRequestModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal) {
        return {};
    }
    switch (section) {
    case NumberColumn:
        return tr("#");
    case TitleColumn:
        return tr("Title");
    case SourceColumn:
        return tr("Source");
    case TargetColumn:
        return tr("Target");
    case StatusColumn:
        return tr("Status");
    case UpdatedColumn:
        return tr("Updated");
    default:
        return {};
    }
}
