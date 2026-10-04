#include "statusservice.h"

#include "daemon/daemonchannel.h"
#include "repositoryservice.h"

namespace {
constexpr int kDefaultAutoRefreshIntervalMs = 5000;
}

StatusService::StatusService(DaemonChannel *channel, RepositoryService *repository, QObject *parent)
    : QObject(parent)
    , channel_(channel)
    , repository_(repository)
{
    timer_.setInterval(kDefaultAutoRefreshIntervalMs);
    connect(&timer_, &QTimer::timeout, this, [this] { refresh(false); });

    connect(repository_, &RepositoryService::activeRepoChanged, this, [this](const RepoInfo &repo) {
        if (repo.root == root_) {
            return;
        }
        root_ = repo.root;
        snapshot_ = {};
        if (root_.isEmpty()) {
            timer_.stop();
            emit statusChanged(root_, snapshot_);
            return;
        }
        refresh(false);
        if (autoRefreshEnabled_) {
            timer_.start();
        }
    });

    connect(channel_, &DaemonChannel::statusFetched, this,
            [this](bool ok, const QString &root, const StatusSnapshot &status, const QString &error) {
                if (root != root_) {
                    return;
                }
                if (!ok) {
                    emit refreshFailed(root, error);
                    return;
                }
                snapshot_ = status;
                emit statusChanged(root_, snapshot_);
            });

    connect(channel_, &DaemonChannel::staged, this,
            [this](bool ok, const QString &root, const StatusSnapshot &status, const QString &error) {
                if (!ok) {
                    emit stageFailed(error);
                    return;
                }
                if (root != root_) {
                    return;
                }
                snapshot_ = status;
                emit statusChanged(root_, snapshot_);
            });
}

QString StatusService::root() const
{
    return root_;
}

StatusSnapshot StatusService::snapshot() const
{
    return snapshot_;
}

void StatusService::setAutoRefreshInterval(int msec)
{
    timer_.setInterval(msec > 0 ? msec : kDefaultAutoRefreshIntervalMs);
}

int StatusService::autoRefreshInterval() const
{
    return timer_.interval();
}

void StatusService::setAutoRefreshEnabled(bool enabled)
{
    autoRefreshEnabled_ = enabled;
    if (!enabled) {
        timer_.stop();
    } else if (!root_.isEmpty()) {
        timer_.start();
    }
}

bool StatusService::autoRefreshEnabled() const
{
    return autoRefreshEnabled_;
}

void StatusService::refresh(bool noCache)
{
    if (root_.isEmpty()) {
        return;
    }
    channel_->fetchStatus(root_, noCache);
}

void StatusService::stage(const QStringList &add, const QStringList &unstage)
{
    if (root_.isEmpty()) {
        return;
    }
    channel_->stage(root_, add, unstage);
}
