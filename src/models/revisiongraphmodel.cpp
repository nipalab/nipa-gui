#include "revisiongraphmodel.h"

#include <QFont>

RevisionGraphModel::RevisionGraphModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

void RevisionGraphModel::setCommits(const QList<CommitInfo> &commits)
{
    beginResetModel();
    commits_ = commits;
    graph_ = layoutGraph(commits);
    endResetModel();
}

void RevisionGraphModel::clear()
{
    beginResetModel();
    commits_.clear();
    graph_.clear();
    endResetModel();
}

QList<CommitInfo> RevisionGraphModel::commits() const
{
    return commits_;
}

int RevisionGraphModel::laneAt(int row) const
{
    if (row < 0 || row >= graph_.size()) {
        return -1;
    }
    return graph_.at(row).lane;
}

QList<RevisionGraphModel::GraphRow> RevisionGraphModel::layoutGraph(const QList<CommitInfo> &commits)
{
    QList<GraphRow> rows;
    QList<QString> lanes; // commit ids expected further down the walk

    for (const CommitInfo &commit : commits) {
        int lane = lanes.indexOf(commit.id);
        if (lane < 0) {
            // A head or a parent that appeared before its child row (not
            // expected in a proper walk, but keep the row visible).
            lanes.prepend(commit.id);
            lane = 0;
        }

        GraphRow row;
        row.lane = lane;
        for (int i = 0; i < lanes.size(); ++i) {
            row.art += (i == lane) ? QChar(0x25CF) : QChar(0x2502);
        }
        rows.append(row);

        lanes.removeAt(lane);
        if (!commit.parent1.isEmpty() && lanes.indexOf(commit.parent1) < 0) {
            lanes.insert(qMin(lane, lanes.size()), commit.parent1);
        }
        if (!commit.parent2.isEmpty() && lanes.indexOf(commit.parent2) < 0) {
            const int insertAt = qMin(lane + 1, lanes.size());
            lanes.insert(insertAt, commit.parent2);
        }
    }
    return rows;
}

int RevisionGraphModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return commits_.size();
}

int RevisionGraphModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return ColumnCount;
}

QVariant RevisionGraphModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= commits_.size()) {
        return {};
    }
    const CommitInfo &commit = commits_.at(index.row());
    const GraphRow &graph = graph_.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case GraphColumn:
            return graph.art;
        case CommitColumn:
            return commit.id.left(8);
        case MessageColumn:
            return commit.message.section(QLatin1Char('\n'), 0, 0);
        case DateColumn:
            return commit.createdAt.toLocalTime().toString(QStringLiteral("yyyy-MM-dd hh:mm"));
        default:
            return {};
        }
    case Qt::ToolTipRole:
        if (commit.parent2.isEmpty()) {
            return tr("%1\nparent: %2").arg(commit.id,
                                            commit.parent1.isEmpty() ? tr("(root)") : commit.parent1);
        }
        return tr("%1\nparents: %2, %3").arg(commit.id, commit.parent1, commit.parent2);
    case Qt::FontRole: {
        QFont font;
        font.setFamilies({QStringLiteral("monospace")});
        if (index.column() == GraphColumn) {
            font.setBold(true);
        }
        return font;
    }
    default:
        return {};
    }
}

QVariant RevisionGraphModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal) {
        return {};
    }
    switch (section) {
    case GraphColumn:
        return tr("Graph");
    case CommitColumn:
        return tr("Commit");
    case MessageColumn:
        return tr("Message");
    case DateColumn:
        return tr("Date");
    default:
        return {};
    }
}
