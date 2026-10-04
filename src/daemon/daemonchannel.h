#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QTimer>

#include <functional>
#include <memory>

#include <grpcpp/grpcpp.h>

#include "daemon.grpc.pb.h"
#include "daemonconnection.h"
#include "daemondiff.h"
#include "daemonoperation.h"
#include "daemontypes.h"

/// Persistent, asynchronous view of the local `nipa serve` daemon.
///
/// Owns the loopback gRPC channel: endpoint discovery from daemon.json, token
/// auth metadata on every RPC, optional auto-spawn/readiness probe and
/// reconnect handling. Every call runs on the QtConcurrent pool and reports
/// back on the channel's thread, so the UI never blocks.
class DaemonChannel : public QObject {
    Q_OBJECT

public:
    enum class State {
        Disconnected, ///< no endpoint (yet); auto-spawn may be pending
        Spawning,     ///< `nipa serve` was started and is publishing its endpoint
        Connecting,   ///< endpoint known, waiting for the first successful ping
        Connected,
        Failed, ///< discovery, spawn or handshake failed
    };
    Q_ENUM(State)

    explicit DaemonChannel(QString configPath = QString(), QObject *parent = nullptr);
    ~DaemonChannel() override;

    void setAutoSpawnEnabled(bool enabled);
    bool autoSpawnEnabled() const;

    void setAutoReconnectEnabled(bool enabled);
    bool autoReconnectEnabled() const;

    /// Emits versionWarning when the daemon is older; empty disables the gate.
    void setMinimumDaemonVersion(const QString &version);
    QString minimumDaemonVersion() const;

    State state() const;
    bool isConnected() const;
    QString configPath() const;

    /// Daemon status from the last successful ping.
    DaemonStatus status() const;

    /// True when this channel started the daemon process itself.
    bool ownsDaemonProcess() const;

    /// Shuts the daemon down (RPC then process) when this channel spawned it.
    /// Runs synchronously with a short deadline; call once at application exit.
    void shutdownIfOwned();

    /// Starts a streamed Update ("Get Latest") for the root.
    DaemonOperation *update(const QString &root);

    /// Starts a streamed Push (Submit) of the root's staged set.
    DaemonOperation *push(const QString &root, const QString &message);

    /// Starts a streamed Diff; the returned object emits text chunks.
    DaemonDiff *diff(const QString &root, const DiffRequestData &request);

    /// Starts a streamed branch Switch (materializes the target head).
    DaemonOperation *switchBranch(const QString &root, const QString &branch);

    /// Starts a streamed Merge of `sourceBranch` into the root's branch. When
    /// `abort` is set the in-progress merge is abandoned instead.
    DaemonOperation *merge(const QString &root,
                           const QString &sourceBranch,
                           bool abort,
                           bool ffOnly,
                           bool noFf,
                           const QString &message);

    /// Starts a streamed Revert of a commit or range, including the
    /// continue/skip/abort resumption modes for conflicted reverts.
    DaemonOperation *revert(const QString &root,
                            const QString &target,
                            bool abort,
                            bool continueOp,
                            bool skip,
                            bool noCommit,
                            int mainline,
                            const QString &message);

    /// Numeric dotted version comparison (-1, 0, 1); tolerates a leading "v".
    static int compareVersions(const QString &a, const QString &b);

public slots:
    void start();
    void reconnect();
    void ping();
    void listRepos();
    void watchRepo(const QString &root);
    void unwatchRepo(const QString &root);
    void fetchStatus(const QString &root, bool noCache = false);
    void fetchTree(const QString &root, const QString &branch, const QStringList &paths);
    void stage(const QString &root, const QStringList &add, const QStringList &unstage);
    void fetchCommitLog(const QString &root,
                        const QString &branch,
                        const QString &startCommitId,
                        int limit);
    void fetchCommit(const QString &root, const QString &commitId);
    void fetchCommitWalk(const QString &root, const QString &startCommitId, int limit);
    void fetchLocks(const QString &root);
    void lockFile(const QString &root, const QString &path, const QString &branch);
    void unlockFile(const QString &root, const QString &path, const QString &branch);
    void fetchBranches(const QString &root);
    void createBranch(const QString &root, const QString &name, const QString &fromBranch);
    void deleteBranch(const QString &root, const QString &name);
    void fetchMergeRequests(const QString &root, const QString &status, int limit);
    void createMergeRequest(const QString &root,
                            const QString &title,
                            const QString &description,
                            const QString &sourceBranch,
                            const QString &targetBranch);
    void mergeMergeRequest(const QString &root, qint64 number);
    void closeMergeRequest(const QString &root, qint64 number);
    void fetchMergeRequestReviews(const QString &root, qint64 number);
    void fetchMergeRequestReviewState(const QString &root, qint64 number);
    void fetchMergeRequestThreads(const QString &root, qint64 number);
    void login(const QString &host, const QString &username, const QString &password);

signals:
    void stateChanged(DaemonChannel::State state, const QString &detail);
    void connected(const DaemonStatus &status);
    void disconnected(const QString &reason);
    void versionWarning(const QString &daemonVersion, const QString &minimumVersion);
    void errorOccurred(const QString &message);

    void pingFinished(bool ok, const DaemonStatus &status, const QString &error);
    void reposListed(bool ok, const QList<RepoInfo> &repos, const QString &error);
    void repoWatched(bool ok, const RepoInfo &repo, const QString &error);
    void repoUnwatched(bool ok, const QString &root, const QString &error);
    void statusFetched(bool ok, const QString &root, const StatusSnapshot &status, const QString &error);
    void treeFetched(bool ok,
                     const QString &root,
                     const QString &branch,
                     const TreeNodeData &rootNode,
                     const QString &error);
    void staged(bool ok, const QString &root, const StatusSnapshot &status, const QString &error);
    void commitLogFetched(bool ok,
                          const QString &root,
                          const QString &branch,
                          const QList<CommitInfo> &commits,
                          const QString &error);
    void commitFetched(bool ok,
                       const QString &root,
                       const CommitInfo &commit,
                       const TreeNodeData &tree,
                       const QString &error);
    void locksFetched(bool ok, const QString &root, const QList<FileLockInfo> &locks, const QString &error);
    void fileLocked(bool ok, const QString &root, const FileLockInfo &lock, const QString &error);
    void fileUnlocked(bool ok, const QString &root, const QString &path, const QString &error);
    void branchesFetched(bool ok, const QString &root, const QList<BranchInfo> &branches, const QString &error);
    void branchCreated(bool ok, const QString &root, const BranchInfo &branch, const QString &error);
    void branchDeleted(bool ok, const QString &root, const QString &name, const QString &error);
    void commitWalkFetched(bool ok,
                           const QString &root,
                           const QList<CommitInfo> &commits,
                           const QString &error);
    void mergeRequestsFetched(bool ok,
                              const QString &root,
                              const QList<MergeRequestInfo> &mergeRequests,
                              const QString &error);
    void mergeRequestCreated(bool ok, const QString &root, const MergeRequestInfo &mergeRequest,
                             const QString &error);
    void mergeRequestMerged(bool ok, const QString &root, const MergeRequestInfo &mergeRequest,
                            const QString &error);
    void mergeRequestClosed(bool ok, const QString &root, const MergeRequestInfo &mergeRequest,
                            const QString &error);
    void mergeRequestReviewsFetched(bool ok,
                                    const QString &root,
                                    qint64 number,
                                    const QList<ReviewInfo> &reviews,
                                    const QString &error);
    void mergeRequestReviewStateFetched(bool ok,
                                        const QString &root,
                                        qint64 number,
                                        const ReviewStateInfo &state,
                                        const QString &error);
    void mergeRequestThreadsFetched(bool ok,
                                    const QString &root,
                                    qint64 number,
                                    const QList<ReviewThreadInfo> &threads,
                                    const QString &error);
    void loginFinished(bool ok, const QString &error);

private:
    void tryConnect();
    bool adoptEndpoint(const DaemonEndpoint &endpoint, QString *error);
    bool spawnDaemon(QString *error);
    void pollSpawnReadiness();
    void onPingSucceeded(const DaemonStatus &status);
    void onPingFailed(grpc::StatusCode code, const QString &error);
    void reportRpcFailure(grpc::StatusCode code, const QString &error);
    void scheduleReconnect();
    void setState(State state, const QString &detail);

    void runAsync(std::function<void()> work, std::function<void()> done);

    using OpReader = grpc::ClientReader<nipadaemon::OpEvent>;
    DaemonOperation *startOperation(
        DaemonOperation::Kind kind,
        std::function<std::unique_ptr<OpReader>(grpc::ClientContext *)> startStream);
    void queueOperationFailure(DaemonOperation *operation, int code, const QString &message);

    QString configPath_;
    bool autoSpawn_ = true;
    bool autoReconnect_ = true;
    bool started_ = false;
    bool spawnedByUs_ = false;
    bool pingInFlight_ = false;
    State state_ = State::Disconnected;
    QString minimumVersion_;

    DaemonStatus status_;
    DaemonEndpoint endpoint_;
    std::shared_ptr<grpc::Channel> channel_;
    std::shared_ptr<nipadaemon::NipaDaemon::Stub> stub_;

    QProcess process_;
    QTimer spawnPollTimer_;
    QTimer reconnectTimer_;
    QElapsedTimer spawnElapsed_;

    static constexpr int kPingTimeoutMs = 3000;
    static constexpr int kQuickRpcTimeoutMs = 15000;
    static constexpr int kStatusRpcTimeoutMs = 60000;
    static constexpr int kSpawnTimeoutMs = 15000;
    static constexpr int kSpawnPollIntervalMs = 500;
    static constexpr int kReconnectDelayMs = 3000;
};
