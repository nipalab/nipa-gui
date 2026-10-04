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
