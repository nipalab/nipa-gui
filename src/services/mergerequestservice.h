#pragma once

#include <QList>
#include <QObject>

#include "daemon/daemontypes.h"

class DaemonChannel;
class RepositoryService;

/// Merge requests of the active repository's project: list (with status
/// filter), selection, create/merge/close, and read-only review state,
/// decisions and inline threads (the daemon exposes no write proxies yet).
class MergeRequestService : public QObject {
    Q_OBJECT

public:
    MergeRequestService(DaemonChannel *channel,
                        RepositoryService *repository,
                        QObject *parent = nullptr);

    QList<MergeRequestInfo> mergeRequests() const;
    MergeRequestInfo selectedMergeRequest() const;
    QList<ReviewInfo> reviews() const;
    ReviewStateInfo reviewState() const;
    QList<ReviewThreadInfo> threads() const;
    QString statusFilter() const;

public slots:
    void refresh();
    void setStatusFilter(const QString &status); // "", open, merged, closed
    void select(qint64 number);
    void create(const QString &title,
                const QString &description,
                const QString &sourceBranch,
                const QString &targetBranch);
    void mergeSelected();
    void closeSelected();

signals:
    void mergeRequestsChanged(const QList<MergeRequestInfo> &mergeRequests);
    void selectionChanged(const MergeRequestInfo &mergeRequest);
    void reviewsChanged(const QList<ReviewInfo> &reviews, const ReviewStateInfo &state);
    void threadsChanged(const QList<ReviewThreadInfo> &threads);
    void mergeRequestCreated(const MergeRequestInfo &mergeRequest);
    void mergeRequestMerged(const MergeRequestInfo &mergeRequest);
    void mergeRequestClosed(const MergeRequestInfo &mergeRequest);
    void errorOccurred(const QString &message);

private:
    static constexpr int kPageLimit = 100;

    DaemonChannel *channel_;
    RepositoryService *repository_;
    QString statusFilter_;
    QList<MergeRequestInfo> mergeRequests_;
    MergeRequestInfo selected_;
    QList<ReviewInfo> reviews_;
    ReviewStateInfo reviewState_;
    QList<ReviewThreadInfo> threads_;
};
