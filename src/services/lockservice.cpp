#include "lockservice.h"

#include "daemon/daemonchannel.h"
#include "repositoryservice.h"

LockService::LockService(DaemonChannel *channel, RepositoryService *repository, QObject *parent)
    : QObject(parent)
    , channel_(channel)
    , repository_(repository)
{
    connect(repository_, &RepositoryService::activeRepoChanged, this, [this](const RepoInfo &repo) {
        locks_.clear();
        emit locksChanged(locks_);
        if (!repo.root.isEmpty()) {
            refresh();
        }
    });

    connect(channel_, &DaemonChannel::locksFetched, this,
            [this](bool ok, const QString &root, const QList<FileLockInfo> &locks, const QString &error) {
                if (!ok) {
                    emit errorOccurred(error);
                    return;
                }
                if (root != repository_->activeRoot()) {
                    return;
                }
                locks_ = locks;
                emit locksChanged(locks_);
            });

    connect(channel_, &DaemonChannel::fileLocked, this,
            [this](bool ok, const QString &, const FileLockInfo &, const QString &error) {
                if (!ok) {
                    emit errorOccurred(error);
                    return;
                }
                refresh();
            });

    connect(channel_, &DaemonChannel::fileUnlocked, this,
            [this](bool ok, const QString &, const QString &, const QString &error) {
                if (!ok) {
                    emit errorOccurred(error);
                    return;
                }
                refresh();
            });
}

QList<FileLockInfo> LockService::locks() const
{
    return locks_;
}

QList<FileLockInfo> LockService::locksForPath(const QString &path) const
{
    QList<FileLockInfo> matches;
    if (path.isEmpty()) {
        return matches;
    }
    for (const FileLockInfo &lock : locks_) {
        if (lock.path == path || path.startsWith(lock.path + QLatin1Char('/'))
            || lock.path.startsWith(path + QLatin1Char('/'))) {
            matches.append(lock);
        }
    }
    return matches;
}

void LockService::refresh()
{
    if (repository_->activeRoot().isEmpty()) {
        return;
    }
    channel_->fetchLocks(repository_->activeRoot());
}

void LockService::lock(const QString &path)
{
    if (path.isEmpty() || repository_->activeRoot().isEmpty()) {
        return;
    }
    channel_->lockFile(repository_->activeRoot(), path, repository_->activeRepo().branch);
}

void LockService::unlock(const QString &path)
{
    if (path.isEmpty() || repository_->activeRoot().isEmpty()) {
        return;
    }
    channel_->unlockFile(repository_->activeRoot(), path, repository_->activeRepo().branch);
}
