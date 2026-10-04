#pragma once

#include <QAbstractTableModel>
#include <QList>

#include "daemon/daemontypes.h"

/// Commit history table (commit, author, date, message) with append-only
/// pagination.
class CommitLogModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column {
        CommitColumn = 0,
        AuthorColumn,
        DateColumn,
        MessageColumn,
        ColumnCount,
    };
    Q_ENUM(Column)

    explicit CommitLogModel(QObject *parent = nullptr);

    void setCommits(const QList<CommitInfo> &commits);
    void appendCommits(const QList<CommitInfo> &commits);
    void clear();

    QList<CommitInfo> commits() const;
    QString commitIdAt(int row) const;
    int rowForCommitId(const QString &commitId) const;

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    QList<CommitInfo> commits_;
};
