#include "historyservice.h"

#include "daemon/daemonchannel.h"
#include "repositoryservice.h"

HistoryService::HistoryService(DaemonChannel *channel, RepositoryService *repository, QObject *parent)
    : QObject(parent)
    , channel_(channel)
    , repository_(repository)
{
    connect(repository_, &RepositoryService::activeRepoChanged, this, [this](const RepoInfo &repo) {
        clear();
        if (!repo.root.isEmpty()) {
            refresh();
        }
    });

    connect(channel_, &DaemonChannel::commitLogFetched, this,
            [this](bool ok, const QString &root, const QString &, const QList<CommitInfo> &commits,
                   const QString &error) {
                if (root != repository_->activeRoot()) {
                    return;
                }
                loading_ = false;
                if (!ok) {
                    emit errorOccurred(error);
                    return;
                }
                if (appendPage_) {
                    commits_ += commits;
                } else {
                    commits_ = commits;
                }
                canLoadMore_ = commits.size() == kPageSize;
                emit commitsChanged(commits_, canLoadMore_);
            });

    connect(channel_, &DaemonChannel::commitFetched, this,
            [this](bool ok, const QString &root, const CommitInfo &commit, const TreeNodeData &tree,
                   const QString &error) {
                if (root != repository_->activeRoot()) {
                    return;
                }
                if (!ok) {
                    emit errorOccurred(error);
                    return;
                }
                if (commit.id != selectedCommitId_) {
                    return;
                }
                selectedCommit_ = commit;
                if (selectedCommit_.authorName.isEmpty() || selectedCommit_.authorEmail.isEmpty()) {
                    for (const CommitInfo &entry : commits_) {
                        if (entry.id == commit.id) {
                            selectedCommit_.authorName = entry.authorName;
                            selectedCommit_.authorEmail = entry.authorEmail;
                            break;
                        }
                    }
                }
                selectedTree_ = tree;
                emit commitDetailChanged(selectedCommit_, selectedTree_);
            });
}

QList<CommitInfo> HistoryService::commits() const
{
    return commits_;
}

bool HistoryService::canLoadMore() const
{
    return canLoadMore_;
}

bool HistoryService::isLoading() const
{
    return loading_;
}

CommitInfo HistoryService::selectedCommit() const
{
    return selectedCommit_;
}

TreeNodeData HistoryService::selectedTree() const
{
    return selectedTree_;
}

void HistoryService::refresh()
{
    if (repository_->activeRoot().isEmpty()) {
        return;
    }
    loading_ = true;
    appendPage_ = false;
    channel_->fetchCommitLog(repository_->activeRoot(), repository_->activeRepo().branch, QString(),
                             kPageSize);
}

void HistoryService::loadMore()
{
    if (!canLoadMore_ || loading_ || commits_.isEmpty()) {
        return;
    }
    loading_ = true;
    appendPage_ = true;
    channel_->fetchCommitLog(repository_->activeRoot(), repository_->activeRepo().branch,
                             commits_.last().id, kPageSize);
}

void HistoryService::selectCommit(const QString &commitId)
{
    if (commitId.isEmpty() || repository_->activeRoot().isEmpty()) {
        return;
    }
    selectedCommitId_ = commitId;
    channel_->fetchCommit(repository_->activeRoot(), commitId);
}

void HistoryService::clear()
{
    commits_.clear();
    canLoadMore_ = false;
    loading_ = false;
    appendPage_ = false;
    selectedCommitId_.clear();
    selectedCommit_ = {};
    selectedTree_ = {};
    emit commitsChanged(commits_, canLoadMore_);
}
