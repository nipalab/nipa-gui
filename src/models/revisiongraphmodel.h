#pragma once

#include <QAbstractTableModel>
#include <QList>

#include "daemon/daemontypes.h"

/// Revision graph table: a text lane rendering ("●│") plus commit, message and
/// date columns. `layoutGraph` assigns each commit a lane while walking the
/// two-parent DAG newest-first.
class RevisionGraphModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column {
        GraphColumn = 0,
        CommitColumn,
        MessageColumn,
        DateColumn,
        ColumnCount,
    };
    Q_ENUM(Column)

    struct GraphRow {
        QString art; // one char per active lane: ● at the node, │ elsewhere
        int lane = 0;
    };

    explicit RevisionGraphModel(QObject *parent = nullptr);

    void setCommits(const QList<CommitInfo> &commits);
    void clear();

    QList<CommitInfo> commits() const;
    int laneAt(int row) const;

    /// Lane assignment for a newest-first, both-parents commit list.
    static QList<GraphRow> layoutGraph(const QList<CommitInfo> &commits);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    QList<CommitInfo> commits_;
    QList<GraphRow> graph_;
};
