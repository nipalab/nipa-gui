#pragma once

#include <QAbstractTableModel>
#include <QList>

#include "daemon/daemontypes.h"

/// Branch table: name, head commit, default and protected flags.
class BranchModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column {
        NameColumn = 0,
        HeadColumn,
        DefaultColumn,
        ProtectedColumn,
        ColumnCount,
    };
    Q_ENUM(Column)

    explicit BranchModel(QObject *parent = nullptr);

    void setBranches(const QList<BranchInfo> &branches);
    void clear();

    QList<BranchInfo> branches() const;
    BranchInfo branchAt(int row) const;
    QString nameAt(int row) const;
    int rowForName(const QString &name) const;

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    QList<BranchInfo> branches_;
};
