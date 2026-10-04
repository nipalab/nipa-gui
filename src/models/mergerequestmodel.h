#pragma once

#include <QAbstractTableModel>
#include <QList>

#include "daemon/daemontypes.h"

/// Merge request table: number, title, source/target branch, status, updated.
class MergeRequestModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column {
        NumberColumn = 0,
        TitleColumn,
        SourceColumn,
        TargetColumn,
        StatusColumn,
        UpdatedColumn,
        ColumnCount,
    };
    Q_ENUM(Column)

    explicit MergeRequestModel(QObject *parent = nullptr);

    void setMergeRequests(const QList<MergeRequestInfo> &mergeRequests);
    void clear();

    QList<MergeRequestInfo> mergeRequests() const;
    MergeRequestInfo mergeRequestAt(int row) const;
    qint64 numberAt(int row) const;
    int rowForNumber(qint64 number) const;

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    QList<MergeRequestInfo> mergeRequests_;
};
