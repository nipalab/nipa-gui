#include "repositoryservice.h"

#include "daemon/daemonchannel.h"

RepositoryService::RepositoryService(DaemonChannel *channel, QObject *parent)
    : QObject(parent)
    , channel_(channel)
{
    connect(channel_, &DaemonChannel::reposListed, this,
            [this](bool ok, const QList<RepoInfo> &repos, const QString &error) {
                if (!ok) {
                    emit errorOccurred(error);
                    return;
                }
                repos_ = repos;
                emit reposChanged(repos_);
            });

    connect(channel_, &DaemonChannel::repoWatched, this,
            [this](bool ok, const RepoInfo &repo, const QString &error) {
                if (!ok) {
                    emit errorOccurred(error);
                    return;
                }
                activeRepo_ = repo;
                emit activeRepoChanged(activeRepo_);
            });

    connect(channel_, &DaemonChannel::repoUnwatched, this,
            [this](bool ok, const QString &root, const QString &error) {
                if (!ok) {
                    emit errorOccurred(error);
                    return;
                }
                if (activeRepo_.root == root) {
                    activeRepo_ = {};
                    emit activeRepoChanged(activeRepo_);
                }
            });
}

QList<RepoInfo> RepositoryService::repos() const
{
    return repos_;
}

RepoInfo RepositoryService::activeRepo() const
{
    return activeRepo_;
}

QString RepositoryService::activeRoot() const
{
    return activeRepo_.root;
}

bool RepositoryService::hasActiveRepo() const
{
    return !activeRepo_.root.isEmpty();
}

void RepositoryService::refresh()
{
    channel_->listRepos();
}

void RepositoryService::open(const QString &root)
{
    if (root.isEmpty()) {
        return;
    }
    channel_->watchRepo(root);
}

void RepositoryService::close()
{
    if (activeRepo_.root.isEmpty()) {
        activeRepo_ = {};
        emit activeRepoChanged(activeRepo_);
        return;
    }
    const QString root = activeRepo_.root;
    activeRepo_ = {};
    emit activeRepoChanged(activeRepo_);
    channel_->unwatchRepo(root);
}
