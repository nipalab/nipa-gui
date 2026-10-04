#pragma once

#include <QAbstractItemModel>
#include <QHash>

#include "daemon/daemontypes.h"

/// Read-only model over the repository tree at the active branch head. The
/// daemon serves the manifest recursively, so the tree is fetched once and
/// children are exposed lazily by the view; no per-directory round trips.
class RepositoryTreeModel : public QAbstractItemModel {
    Q_OBJECT

public:
    enum Column {
        NameColumn = 0,
        SizeColumn = 1,
        ColumnCount = 2,
    };
    Q_ENUM(Column)

    enum Role {
        PathRole = Qt::UserRole + 1,
        IsDirectoryRole,
    };

    explicit RepositoryTreeModel(QObject *parent = nullptr);

    void setTree(const TreeNodeData &root);
    void clear();
    bool isEmpty() const;

    /// Repository-relative path of the node at `index` ("" for the root).
    QString pathForIndex(const QModelIndex &index) const;
    bool isDirectory(const QModelIndex &index) const;

    QModelIndex index(int row, int column, const QModelIndex &parent = {}) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

private:
    const TreeNodeData *nodeForIndex(const QModelIndex &index) const;
    void indexChildren(const TreeNodeData *node, const TreeNodeData *parent);
    static void sortChildren(TreeNodeData *node);
    static QString formatSize(qint64 bytes);

    TreeNodeData root_;
    QHash<const TreeNodeData *, const TreeNodeData *> parentOf_;
    QHash<const TreeNodeData *, int> rowOf_;
    bool hasTree_ = false;
};
