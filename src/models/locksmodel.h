#pragma once

#include <QAbstractTableModel>
#include <QList>

#include "daemon/daemontypes.h"

/// Binary file locks table: path, scope (branch or mainline), holder, time.
class LocksModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column {
        PathColumn = 0,
        ScopeColumn,
        HolderColumn,
        AcquiredColumn,
        ColumnCount,
    };
    Q_ENUM(Column)

    explicit LocksModel(QObject *parent = nullptr);

    void setLocks(const QList<FileLockInfo> &locks);
    void clear();

    QList<FileLockInfo> locks() const;
    FileLockInfo lockAt(int row) const;
    QString pathAt(int row) const;

    static QString scopeLabel(const FileLockInfo &lock);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    QList<FileLockInfo> locks_;
};
