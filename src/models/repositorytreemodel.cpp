#include "repositorytreemodel.h"

#include <QApplication>
#include <QBrush>
#include <QColor>
#include <QStyle>

#include <algorithm>

RepositoryTreeModel::RepositoryTreeModel(QObject *parent)
    : QAbstractItemModel(parent)
{
}

void RepositoryTreeModel::setTree(const TreeNodeData &root)
{
    beginResetModel();
    root_ = root;
    parentOf_.clear();
    rowOf_.clear();
    sortChildren(&root_);
    indexChildren(&root_, nullptr);
    hasTree_ = true;
    endResetModel();
}

void RepositoryTreeModel::clear()
{
    beginResetModel();
    root_ = {};
    parentOf_.clear();
    rowOf_.clear();
    hasTree_ = false;
    endResetModel();
}

bool RepositoryTreeModel::isEmpty() const
{
    return !hasTree_ || root_.children.isEmpty();
}

void RepositoryTreeModel::indexChildren(const TreeNodeData *node, const TreeNodeData *parent)
{
    for (int row = 0; row < node->children.size(); ++row) {
        const TreeNodeData *child = &node->children.at(row);
        if (parent != nullptr) {
            parentOf_.insert(child, parent);
        }
        rowOf_.insert(child, row);
        indexChildren(child, child);
    }
}

void RepositoryTreeModel::sortChildren(TreeNodeData *node)
{
    std::stable_sort(node->children.begin(), node->children.end(),
                     [](const TreeNodeData &a, const TreeNodeData &b) {
                         if (a.isDirectory != b.isDirectory) {
                             return a.isDirectory;
                         }
                         return QString::compare(a.name, b.name, Qt::CaseInsensitive) < 0;
                     });
    for (TreeNodeData &child : node->children) {
        sortChildren(&child);
    }
}

QString RepositoryTreeModel::pathForIndex(const QModelIndex &index) const
{
    const TreeNodeData *node = nodeForIndex(index);
    return node != nullptr ? node->path : QString();
}

bool RepositoryTreeModel::isDirectory(const QModelIndex &index) const
{
    const TreeNodeData *node = nodeForIndex(index);
    return node != nullptr && node->isDirectory;
}

const TreeNodeData *RepositoryTreeModel::nodeForIndex(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return hasTree_ ? &root_ : nullptr;
    }
    return static_cast<const TreeNodeData *>(index.internalPointer());
}

QModelIndex RepositoryTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasTree_ || column < 0 || column >= ColumnCount || row < 0) {
        return {};
    }
    const TreeNodeData *parentNode = nodeForIndex(parent);
    if (parentNode == nullptr || row >= parentNode->children.size()) {
        return {};
    }
    return createIndex(row, column, const_cast<TreeNodeData *>(&parentNode->children.at(row)));
}

QModelIndex RepositoryTreeModel::parent(const QModelIndex &child) const
{
    if (!child.isValid()) {
        return {};
    }
    const TreeNodeData *node = static_cast<const TreeNodeData *>(child.internalPointer());
    const TreeNodeData *parentNode = parentOf_.value(node, nullptr);
    if (parentNode == nullptr || parentNode == &root_) {
        return {};
    }
    return createIndex(rowOf_.value(parentNode, 0), child.column(),
                       const_cast<TreeNodeData *>(parentNode));
}

int RepositoryTreeModel::rowCount(const QModelIndex &parent) const
{
    if (!hasTree_ || parent.column() > 0) {
        return 0;
    }
    const TreeNodeData *node = nodeForIndex(parent);
    return node != nullptr ? node->children.size() : 0;
}

int RepositoryTreeModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return ColumnCount;
}

QVariant RepositoryTreeModel::data(const QModelIndex &index, int role) const
{
    const TreeNodeData *node = nodeForIndex(index);
    if (node == nullptr || index.internalPointer() == nullptr) {
        return {};
    }

    switch (role) {
    case Qt::DisplayRole:
        if (index.column() == NameColumn) {
            return node->name;
        }
        if (index.column() == SizeColumn && !node->isDirectory) {
            return formatSize(node->sizeBytes);
        }
        return {};
    case Qt::DecorationRole:
        if (index.column() == NameColumn) {
            const QStyle *style = QApplication::style();
            return style->standardIcon(node->isDirectory ? QStyle::SP_DirIcon : QStyle::SP_FileIcon);
        }
        return {};
    case Qt::ToolTipRole:
        return node->isDirectory ? node->path
                                 : tr("%1\n%2\n%3").arg(node->path, formatSize(node->sizeBytes),
                                                        node->isBinary ? tr("binary") : tr("text"));
    case Qt::TextAlignmentRole:
        if (index.column() == SizeColumn) {
            return QVariant::fromValue(Qt::AlignRight | Qt::AlignVCenter);
        }
        return QVariant::fromValue(Qt::AlignLeft | Qt::AlignVCenter);
    case Qt::ForegroundRole:
        if (node->readOnly && !node->isDirectory) {
            return QBrush(QColor(0x6e, 0x77, 0x81));
        }
        return {};
    case PathRole:
        return node->path;
    case IsDirectoryRole:
        return node->isDirectory;
    default:
        return {};
    }
}

QVariant RepositoryTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal) {
        return {};
    }
    switch (section) {
    case NameColumn:
        return tr("Name");
    case SizeColumn:
        return tr("Size");
    default:
        return {};
    }
}

Qt::ItemFlags RepositoryTreeModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QString RepositoryTreeModel::formatSize(qint64 bytes)
{
    constexpr qint64 kKiB = 1024;
    constexpr qint64 kMiB = kKiB * 1024;
    constexpr qint64 kGiB = kMiB * 1024;

    if (bytes < kKiB) {
        return tr("%1 B").arg(bytes);
    }
    if (bytes < kMiB) {
        return tr("%1 KiB").arg(bytes / static_cast<double>(kKiB), 0, 'f', 1);
    }
    if (bytes < kGiB) {
        return tr("%1 MiB").arg(bytes / static_cast<double>(kMiB), 0, 'f', 1);
    }
    return tr("%1 GiB").arg(bytes / static_cast<double>(kGiB), 0, 'f', 1);
}
