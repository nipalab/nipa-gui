#include "mergerequestservice.h"

#include "daemon/daemonchannel.h"
#include "repositoryservice.h"

MergeRequestService::MergeRequestService(DaemonChannel *channel,
                                         RepositoryService *repository,
                                         QObject *parent)
    : QObject(parent)
    , channel_(channel)
    , repository_(repository)
{
    connect(repository_, &RepositoryService::activeRepoChanged, this, [this](const RepoInfo &repo) {
        mergeRequests_.clear();
        selected_ = {};
        reviews_.clear();
        reviewState_ = {};
        threads_.clear();
        emit mergeRequestsChanged(mergeRequests_);
        emit selectionChanged(selected_);
        if (!repo.root.isEmpty()) {
            refresh();
        }
    });

    connect(channel_, &DaemonChannel::mergeRequestsFetched, this,
            [this](bool ok, const QString &root, const QList<MergeRequestInfo> &mergeRequests,
                   const QString &error) {
                if (!ok) {
                    emit errorOccurred(error);
                    return;
                }
                if (root != repository_->activeRoot()) {
                    return;
                }
                mergeRequests_ = mergeRequests;
                emit mergeRequestsChanged(mergeRequests_);
            });

    connect(channel_, &DaemonChannel::mergeRequestCreated, this,
            [this](bool ok, const QString &, const MergeRequestInfo &mergeRequest,
                   const QString &error) {
                if (!ok) {
                    emit errorOccurred(error);
                    return;
                }
                emit mergeRequestCreated(mergeRequest);
                refresh();
            });

    connect(channel_, &DaemonChannel::mergeRequestMerged, this,
            [this](bool ok, const QString &, const MergeRequestInfo &mergeRequest,
                   const QString &error) {
                if (!ok) {
                    emit errorOccurred(error);
                    return;
                }
                emit mergeRequestMerged(mergeRequest);
                refresh();
            });

    connect(channel_, &DaemonChannel::mergeRequestClosed, this,
            [this](bool ok, const QString &, const MergeRequestInfo &mergeRequest,
                   const QString &error) {
                if (!ok) {
                    emit errorOccurred(error);
                    return;
                }
                emit mergeRequestClosed(mergeRequest);
                refresh();
            });

    connect(channel_, &DaemonChannel::mergeRequestReviewsFetched, this,
            [this](bool ok, const QString &root, qint64 number, const QList<ReviewInfo> &reviews,
                   const QString &error) {
                if (!ok) {
                    emit errorOccurred(error);
                    return;
                }
                if (root != repository_->activeRoot() || number != selected_.number) {
                    return;
                }
                reviews_ = reviews;
                emit reviewsChanged(reviews_, reviewState_);
            });

    connect(channel_, &DaemonChannel::mergeRequestReviewStateFetched, this,
            [this](bool ok, const QString &root, qint64 number, const ReviewStateInfo &state,
                   const QString &error) {
                if (!ok) {
                    emit errorOccurred(error);
                    return;
                }
                if (root != repository_->activeRoot() || number != selected_.number) {
                    return;
                }
                reviewState_ = state;
                emit reviewsChanged(reviews_, reviewState_);
            });

    connect(channel_, &DaemonChannel::mergeRequestThreadsFetched, this,
            [this](bool ok, const QString &root, qint64 number,
                   const QList<ReviewThreadInfo> &threads, const QString &error) {
                if (!ok) {
                    emit errorOccurred(error);
                    return;
                }
                if (root != repository_->activeRoot() || number != selected_.number) {
                    return;
                }
                threads_ = threads;
                emit threadsChanged(threads_);
            });
}

QList<MergeRequestInfo> MergeRequestService::mergeRequests() const
{
    return mergeRequests_;
}

MergeRequestInfo MergeRequestService::selectedMergeRequest() const
{
    return selected_;
}

QList<ReviewInfo> MergeRequestService::reviews() const
{
    return reviews_;
}

ReviewStateInfo MergeRequestService::reviewState() const
{
    return reviewState_;
}

QList<ReviewThreadInfo> MergeRequestService::threads() const
{
    return threads_;
}

QString MergeRequestService::statusFilter() const
{
    return statusFilter_;
}

void MergeRequestService::refresh()
{
    if (repository_->activeRoot().isEmpty()) {
        return;
    }
    channel_->fetchMergeRequests(repository_->activeRoot(), statusFilter_, kPageLimit);
}

void MergeRequestService::setStatusFilter(const QString &status)
{
    if (statusFilter_ == status) {
        return;
    }
    statusFilter_ = status;
    refresh();
}

void MergeRequestService::select(qint64 number)
{
    if (number <= 0 || repository_->activeRoot().isEmpty()) {
        return;
    }
    for (const MergeRequestInfo &mergeRequest : mergeRequests_) {
        if (mergeRequest.number == number) {
            selected_ = mergeRequest;
            break;
        }
    }
    reviews_.clear();
    reviewState_ = {};
    threads_.clear();
    emit selectionChanged(selected_);
    emit reviewsChanged(reviews_, reviewState_);
    emit threadsChanged(threads_);
    channel_->fetchMergeRequestReviews(repository_->activeRoot(), number);
    channel_->fetchMergeRequestReviewState(repository_->activeRoot(), number);
    channel_->fetchMergeRequestThreads(repository_->activeRoot(), number);
}

void MergeRequestService::create(const QString &title,
                                 const QString &description,
                                 const QString &sourceBranch,
                                 const QString &targetBranch)
{
    if (title.isEmpty() || sourceBranch.isEmpty() || targetBranch.isEmpty()
        || repository_->activeRoot().isEmpty()) {
        return;
    }
    channel_->createMergeRequest(repository_->activeRoot(), title, description, sourceBranch,
                                 targetBranch);
}

void MergeRequestService::mergeSelected()
{
    if (selected_.number <= 0 || selected_.status != QLatin1String("open")
        || repository_->activeRoot().isEmpty()) {
        return;
    }
    channel_->mergeMergeRequest(repository_->activeRoot(), selected_.number);
}

void MergeRequestService::closeSelected()
{
    if (selected_.number <= 0 || selected_.status != QLatin1String("open")
        || repository_->activeRoot().isEmpty()) {
        return;
    }
    channel_->closeMergeRequest(repository_->activeRoot(), selected_.number);
}
