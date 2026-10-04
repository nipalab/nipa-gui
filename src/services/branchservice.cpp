#include "branchservice.h"

#include "daemon/daemonchannel.h"
#include "repositoryservice.h"

BranchService::BranchService(DaemonChannel *channel, RepositoryService *repository, QObject *parent)
    : QObject(parent)
    , channel_(channel)
    , repository_(repository)
{
    connect(repository_, &RepositoryService::activeRepoChanged, this, [this](const RepoInfo &repo) {
        branches_.clear();
        emit branchesChanged(branches_);
        if (!repo.root.isEmpty()) {
            refresh();
        }
    });

    connect(channel_, &DaemonChannel::branchesFetched, this,
            [this](bool ok, const QString &root, const QList<BranchInfo> &branches, const QString &error) {
                if (!ok) {
                    emit errorOccurred(error);
                    return;
                }
                if (root != repository_->activeRoot()) {
                    return;
                }
                branches_ = branches;
                emit branchesChanged(branches_);
            });

    connect(channel_, &DaemonChannel::branchCreated, this,
            [this](bool ok, const QString &, const BranchInfo &, const QString &error) {
                if (!ok) {
                    emit errorOccurred(error);
                    return;
                }
                refresh();
            });

    connect(channel_, &DaemonChannel::branchDeleted, this,
            [this](bool ok, const QString &, const QString &, const QString &error) {
                if (!ok) {
                    emit errorOccurred(error);
                    return;
                }
                refresh();
            });
}

QList<BranchInfo> BranchService::branches() const
{
    return branches_;
}

QString BranchService::activeBranchName() const
{
    return repository_->activeRepo().branch;
}

BranchInfo BranchService::activeBranch() const
{
    const QString name = activeBranchName();
    for (const BranchInfo &branch : branches_) {
        if (branch.name == name) {
            return branch;
        }
    }
    for (const BranchInfo &branch : branches_) {
        if (branch.isDefault) {
            return branch;
        }
    }
    return branches_.isEmpty() ? BranchInfo{} : branches_.first();
}

void BranchService::refresh()
{
    if (repository_->activeRoot().isEmpty()) {
        return;
    }
    channel_->fetchBranches(repository_->activeRoot());
}

void BranchService::create(const QString &name, const QString &fromBranch)
{
    if (name.isEmpty() || repository_->activeRoot().isEmpty()) {
        return;
    }
    channel_->createBranch(repository_->activeRoot(), name, fromBranch);
}

void BranchService::remove(const QString &name)
{
    if (name.isEmpty() || repository_->activeRoot().isEmpty()) {
        return;
    }
    channel_->deleteBranch(repository_->activeRoot(), name);
}
