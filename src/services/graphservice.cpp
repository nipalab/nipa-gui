#include "graphservice.h"

#include "branchservice.h"
#include "daemon/daemonchannel.h"
#include "repositoryservice.h"

GraphService::GraphService(DaemonChannel *channel,
                           RepositoryService *repository,
                           BranchService *branches,
                           QObject *parent)
    : QObject(parent)
    , channel_(channel)
    , repository_(repository)
    , branches_(branches)
{
    connect(repository_, &RepositoryService::activeRepoChanged, this, [this](const RepoInfo &repo) {
        clear();
        if (!repo.root.isEmpty()) {
            refresh();
        }
    });

    // The branch head moves on switch/update/merge and after branch changes;
    // re-walk whenever the branch list is reloaded.
    connect(branches_, &BranchService::branchesChanged, this, [this](const QList<BranchInfo> &) {
        if (!repository_->activeRoot().isEmpty()) {
            refresh();
        }
    });

    connect(channel_, &DaemonChannel::commitWalkFetched, this,
            [this](bool ok, const QString &root, const QList<CommitInfo> &commits, const QString &error) {
                if (!ok) {
                    emit errorOccurred(error);
                    return;
                }
                if (root != repository_->activeRoot()) {
                    return;
                }
                commits_ = commits;
                emit graphChanged(commits_);
            });
}

QList<CommitInfo> GraphService::commits() const
{
    return commits_;
}

void GraphService::refresh()
{
    if (repository_->activeRoot().isEmpty()) {
        return;
    }
    const QString start = branches_->activeBranch().commitId;
    if (start.isEmpty()) {
        clear();
        return;
    }
    channel_->fetchCommitWalk(repository_->activeRoot(), start, kGraphLimit);
}

void GraphService::clear()
{
    commits_.clear();
    emit graphChanged(commits_);
}
