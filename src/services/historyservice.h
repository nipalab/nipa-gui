#pragma once

#include <QList>
#include <QObject>

#include "daemon/daemontypes.h"

class DaemonChannel;
class RepositoryService;

/// Commit history of the active repository's branch: first page, "load more"
/// pagination via the last commit id cursor, and commit detail (message plus
/// the recursive tree at that commit).
class HistoryService : public QObject {
    Q_OBJECT

public:
    HistoryService(DaemonChannel *channel, RepositoryService *repository, QObject *parent = nullptr);

    QList<CommitInfo> commits() const;
    bool canLoadMore() const;
    bool isLoading() const;
    CommitInfo selectedCommit() const;
    TreeNodeData selectedTree() const;

public slots:
    void refresh();
    void loadMore();
    void selectCommit(const QString &commitId);
    void clear();

signals:
    void commitsChanged(const QList<CommitInfo> &commits, bool canLoadMore);
    void commitDetailChanged(const CommitInfo &commit, const TreeNodeData &tree);
    void errorOccurred(const QString &message);

private:
    static constexpr int kPageSize = 50;

    DaemonChannel *channel_;
    RepositoryService *repository_;
    QList<CommitInfo> commits_;
    bool canLoadMore_ = false;
    bool loading_ = false;
    bool appendPage_ = false;
    QString selectedCommitId_;
    CommitInfo selectedCommit_;
    TreeNodeData selectedTree_;
};
