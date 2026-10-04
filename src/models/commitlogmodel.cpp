#include "commitlogmodel.h"

#include <QFont>

CommitLogModel::CommitLogModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

void CommitLogModel::setCommits(const QList<CommitInfo> &commits)
{
    beginResetModel();
    commits_ = commits;
    endResetModel();
}

void CommitLogModel::appendCommits(const QList<CommitInfo> &commits)
{
    if (commits.isEmpty()) {
        return;
    }
    beginInsertRows({}, commits_.size(), commits_.size() + commits.size() - 1);
    commits_ += commits;
    endInsertRows();
}

void CommitLogModel::clear()
{
    beginResetModel();
    commits_.clear();
    endResetModel();
}

QList<CommitInfo> CommitLogModel::commits() const
{
    return commits_;
}

QString CommitLogModel::commitIdAt(int row) const
{
    if (row < 0 || row >= commits_.size()) {
        return {};
    }
    return commits_.at(row).id;
}

int CommitLogModel::rowForCommitId(const QString &commitId) const
{
    for (int row = 0; row < commits_.size(); ++row) {
        if (commits_.at(row).id == commitId) {
            return row;
        }
    }
    return -1;
}

int CommitLogModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return commits_.size();
}

int CommitLogModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return ColumnCount;
}

QVariant CommitLogModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= commits_.size()) {
        return {};
    }
    const CommitInfo &commit = commits_.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case CommitColumn:
            return commit.id.left(8);
        case AuthorColumn:
            return commit.authorName;
        case DateColumn:
            return commit.createdAt.toLocalTime().toString(QStringLiteral("yyyy-MM-dd hh:mm"));
        case MessageColumn:
            return commit.message.section(QLatin1Char('\n'), 0, 0);
        default:
            return {};
        }
    case Qt::ToolTipRole:
        return tr("%1\n%2\n%3 <%4>").arg(commit.id, commit.hash, commit.authorName, commit.authorEmail);
    case Qt::FontRole:
        if (index.column() == CommitColumn) {
            QFont font;
            font.setFamilies({QStringLiteral("monospace")});
            return font;
        }
        return {};
    default:
        return {};
    }
}

QVariant CommitLogModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal) {
        return {};
    }
    switch (section) {
    case CommitColumn:
        return tr("Commit");
    case AuthorColumn:
        return tr("Author");
    case DateColumn:
        return tr("Date");
    case MessageColumn:
        return tr("Message");
    default:
        return {};
    }
}
