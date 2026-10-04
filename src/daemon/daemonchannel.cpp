#include "daemonchannel.h"

#include <QFutureWatcher>
#include <QStandardPaths>

#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <chrono>

namespace {

QString rpcErrorText(const grpc::Status &status)
{
    const QString message = QString::fromStdString(status.error_message());
    if (status.error_code() == grpc::StatusCode::UNAVAILABLE) {
        return QObject::tr("the daemon is not answering (%1)").arg(message);
    }
    return QObject::tr("the daemon rejected the request: %1").arg(message);
}

void addAuthMetadata(grpc::ClientContext *context, const DaemonEndpoint &endpoint)
{
    context->AddMetadata("x-nipa-daemon-token", endpoint.token.toStdString());
}

void setDeadline(grpc::ClientContext *context, int timeoutMs)
{
    context->set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(timeoutMs));
}

RepoInfo repoFromProto(const nipadaemon::RepoInfo &repo)
{
    RepoInfo info;
    info.root = QString::fromStdString(repo.root());
    info.url = QString::fromStdString(repo.url());
    info.branch = QString::fromStdString(repo.branch());
    for (const auto &prefix : repo.sparse()) {
        info.sparse.append(QString::fromStdString(prefix));
    }
    return info;
}

StatusSnapshot statusFromProto(const nipadaemon::StatusResponse &status)
{
    StatusSnapshot snapshot;
    for (const auto &path : status.staged()) {
        snapshot.staged.append(QString::fromStdString(path));
    }
    for (const auto &path : status.deleted()) {
        snapshot.deleted.append(QString::fromStdString(path));
    }
    for (const auto &path : status.modified()) {
        snapshot.modified.append(QString::fromStdString(path));
    }
    for (const auto &path : status.untracked()) {
        snapshot.untracked.append(QString::fromStdString(path));
    }
    for (const auto &path : status.missing()) {
        snapshot.missing.append(QString::fromStdString(path));
    }
    for (const auto &path : status.conflicts()) {
        snapshot.conflicts.append(QString::fromStdString(path));
    }
    snapshot.branch = QString::fromStdString(status.branch());
    if (status.has_head()) {
        snapshot.headKind = QString::fromStdString(status.head().kind());
        snapshot.headName = QString::fromStdString(status.head().name());
    }
    return snapshot;
}

QList<int> versionSegments(const QString &version)
{
    QString cleaned = version.trimmed();
    if (cleaned.startsWith(QLatin1Char('v')) || cleaned.startsWith(QLatin1Char('V'))) {
        cleaned.remove(0, 1);
    }

    // Drop prerelease/build suffixes ("1.2.3-rc1+5" -> "1.2.3").
    for (int i = 0; i < cleaned.size(); ++i) {
        const QChar c = cleaned.at(i);
        if (!c.isDigit() && c != QLatin1Char('.')) {
            cleaned.truncate(i);
            break;
        }
    }

    QList<int> segments;
    for (const QString &part : cleaned.split(QLatin1Char('.'))) {
        if (part.isEmpty()) {
            break;
        }
        segments.append(part.toInt());
    }
    return segments;
}

} // namespace

DaemonChannel::DaemonChannel(QString configPath, QObject *parent)
    : QObject(parent)
    , configPath_(std::move(configPath))
{
    qRegisterMetaType<DaemonStatus>();
    qRegisterMetaType<RepoInfo>();
    qRegisterMetaType<StatusSnapshot>();
    qRegisterMetaType<QList<RepoInfo>>();

    spawnPollTimer_.setSingleShot(false);
    spawnPollTimer_.setInterval(kSpawnPollIntervalMs);
    connect(&spawnPollTimer_, &QTimer::timeout, this, &DaemonChannel::pollSpawnReadiness);

    reconnectTimer_.setSingleShot(true);
    reconnectTimer_.setInterval(kReconnectDelayMs);
    connect(&reconnectTimer_, &QTimer::timeout, this, &DaemonChannel::reconnect);

    connect(&process_, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus status) {
        if (!spawnedByUs_) {
            return;
        }
        if (state_ != State::Disconnected && state_ != State::Failed) {
            const QString reason =
                status == QProcess::CrashExit
                    ? tr("the spawned `nipa serve` process crashed")
                    : tr("the spawned `nipa serve` process exited (code %1)").arg(exitCode);
            setState(State::Disconnected, reason);
            if (started_) {
                emit disconnected(reason);
                emit errorOccurred(reason);
            }
        }
    });
}

DaemonChannel::~DaemonChannel()
{
    shutdownIfOwned();
}

void DaemonChannel::setAutoSpawnEnabled(bool enabled)
{
    autoSpawn_ = enabled;
}

bool DaemonChannel::autoSpawnEnabled() const
{
    return autoSpawn_;
}

void DaemonChannel::setAutoReconnectEnabled(bool enabled)
{
    autoReconnect_ = enabled;
}

bool DaemonChannel::autoReconnectEnabled() const
{
    return autoReconnect_;
}

void DaemonChannel::setMinimumDaemonVersion(const QString &version)
{
    minimumVersion_ = version;
}

QString DaemonChannel::minimumDaemonVersion() const
{
    return minimumVersion_;
}

DaemonChannel::State DaemonChannel::state() const
{
    return state_;
}

bool DaemonChannel::isConnected() const
{
    return state_ == State::Connected;
}

QString DaemonChannel::configPath() const
{
    return DaemonConnection(configPath_).configPath();
}

DaemonStatus DaemonChannel::status() const
{
    return status_;
}

bool DaemonChannel::ownsDaemonProcess() const
{
    return spawnedByUs_;
}

void DaemonChannel::runAsync(std::function<void()> work, std::function<void()> done)
{
    auto *watcher = new QFutureWatcher<void>(this);
    connect(
        watcher,
        &QFutureWatcher<void>::finished,
        this,
        [watcher, done = std::move(done)]() mutable {
            watcher->deleteLater();
            done();
        });
    watcher->setFuture(QtConcurrent::run(std::move(work)));
}

void DaemonChannel::setState(State state, const QString &detail)
{
    if (state_ == state && detail.isEmpty()) {
        return;
    }
    state_ = state;
    emit stateChanged(state, detail);
}

void DaemonChannel::start()
{
    started_ = true;
    tryConnect();
}

void DaemonChannel::reconnect()
{
    spawnPollTimer_.stop();
    reconnectTimer_.stop();
    if (spawnedByUs_ && process_.state() == QProcess::NotRunning) {
        spawnedByUs_ = false;
    }
    channel_.reset();
    stub_.reset();
    endpoint_ = {};
    status_ = {};
    tryConnect();
}

void DaemonChannel::tryConnect()
{
    DaemonConnection discovery(configPath_);
    DaemonEndpoint endpoint;
    QString error;
    if (discovery.load(&endpoint, &error)) {
        if (!adoptEndpoint(endpoint, &error)) {
            setState(State::Failed, error);
            emit errorOccurred(error);
            return;
        }
        setState(State::Connecting, tr("Connecting to %1…").arg(endpoint.address()));
        ping();
        return;
    }

    if (autoSpawn_) {
        QString spawnError;
        if (!spawnDaemon(&spawnError)) {
            setState(State::Failed, spawnError);
            emit errorOccurred(spawnError);
        }
        return;
    }

    setState(State::Disconnected, error);
    emit errorOccurred(error);
}

bool DaemonChannel::adoptEndpoint(const DaemonEndpoint &endpoint, QString *error)
{
    if (!endpoint.isValid()) {
        *error = tr("the daemon endpoint is incomplete");
        return false;
    }
    channel_ = grpc::CreateChannel(endpoint.address().toStdString(), grpc::InsecureChannelCredentials());
    stub_ = nipadaemon::NipaDaemon::NewStub(channel_);
    endpoint_ = endpoint;
    return true;
}

bool DaemonChannel::spawnDaemon(QString *error)
{
    if (process_.state() == QProcess::Running) {
        // Our daemon is already starting; keep polling instead of forking again.
        if (!spawnPollTimer_.isActive()) {
            spawnElapsed_.restart();
            spawnPollTimer_.start();
        }
        setState(State::Spawning, tr("Waiting for `nipa serve` to publish its endpoint…"));
        return true;
    }

    const QString executable = QStandardPaths::findExecutable(QStringLiteral("nipa"));
    if (executable.isEmpty()) {
        *error = tr("`nipa` was not found in PATH - start the daemon with `nipa serve`.");
        return false;
    }

    process_.setProgram(executable);
    process_.setArguments({QStringLiteral("serve")});
    process_.start();
    if (!process_.waitForStarted(3000)) {
        *error = tr("failed to start `nipa serve`: %1").arg(process_.errorString());
        process_.close();
        return false;
    }

    spawnedByUs_ = true;
    setState(State::Spawning,
             tr("Started `nipa serve` (pid %1); waiting for readiness…").arg(process_.processId()));
    spawnElapsed_.start();
    spawnPollTimer_.start();
    QTimer::singleShot(0, this, &DaemonChannel::pollSpawnReadiness);
    return true;
}

void DaemonChannel::pollSpawnReadiness()
{
    if (!spawnPollTimer_.isActive()) {
        return;
    }
    if (spawnElapsed_.elapsed() > kSpawnTimeoutMs) {
        spawnPollTimer_.stop();
        const QString message = tr("timed out waiting for `nipa serve` to publish its endpoint");
        setState(State::Failed, message);
        emit errorOccurred(message);
        return;
    }
    if (pingInFlight_) {
        return;
    }

    DaemonConnection discovery(configPath_);
    DaemonEndpoint endpoint;
    QString error;
    if (!discovery.load(&endpoint, &error)) {
        return; // not published yet
    }
    if (endpoint.port == endpoint_.port && endpoint_.port > 0 && state_ == State::Connecting) {
        return; // same endpoint already being probed by an in-flight ping
    }
    if (!adoptEndpoint(endpoint, &error)) {
        return;
    }
    setState(State::Connecting, tr("Waiting for the daemon to answer on %1…").arg(endpoint.address()));
    ping();
}

void DaemonChannel::ping()
{
    if (!stub_) {
        emit pingFinished(false, {}, tr("the daemon endpoint is not available"));
        return;
    }

    struct Outcome {
        bool ok = false;
        DaemonStatus status;
        QString error;
        grpc::StatusCode code = grpc::StatusCode::OK;
    };
    auto outcome = std::make_shared<Outcome>();
    auto stub = stub_;
    auto endpoint = endpoint_;
    pingInFlight_ = true;

    runAsync(
        [stub, endpoint, outcome] {
            grpc::ClientContext context;
            addAuthMetadata(&context, endpoint);
            setDeadline(&context, kPingTimeoutMs);

            nipadaemon::PingRequest request;
            nipadaemon::PingResponse response;
            const grpc::Status result = stub->Ping(&context, request, &response);
            outcome->ok = result.ok();
            outcome->code = result.error_code();
            if (result.ok()) {
                outcome->status.version = QString::fromStdString(response.version());
                outcome->status.pid = response.pid();
                outcome->status.endpoint = endpoint.address();
            } else {
                outcome->error = rpcErrorText(result);
            }
        },
        [this, outcome] {
            pingInFlight_ = false;
            if (outcome->ok) {
                onPingSucceeded(outcome->status);
            } else {
                onPingFailed(outcome->code, outcome->error);
            }
            emit pingFinished(outcome->ok, outcome->status, outcome->error);
        });
}

void DaemonChannel::onPingSucceeded(const DaemonStatus &status)
{
    spawnPollTimer_.stop();
    reconnectTimer_.stop();

    const bool firstConnect = state_ != State::Connected;
    status_ = status;
    setState(State::Connected, tr("Connected to nipa daemon %1 (pid %2) at %3")
                                   .arg(status.version, QString::number(status.pid), status.endpoint));
    if (firstConnect) {
        emit connected(status);
    }
    if (!minimumVersion_.isEmpty() && compareVersions(status.version, minimumVersion_) < 0) {
        emit versionWarning(status.version, minimumVersion_);
    }
}

void DaemonChannel::onPingFailed(grpc::StatusCode code, const QString &error)
{
    if (spawnPollTimer_.isActive()) {
        // Still waiting for the daemon we spawned; a stale endpoint may answer
        // first. Keep polling until the timeout.
        return;
    }

    if (code == grpc::StatusCode::UNAVAILABLE && autoSpawn_ && !spawnedByUs_) {
        QString spawnError;
        if (spawnDaemon(&spawnError)) {
            return;
        }
        setState(State::Failed, spawnError);
        emit errorOccurred(spawnError);
        return;
    }

    if (state_ == State::Connected) {
        emit disconnected(error);
        setState(State::Disconnected, error);
        scheduleReconnect();
    } else {
        setState(State::Failed, error);
        emit errorOccurred(error);
    }
}

void DaemonChannel::reportRpcFailure(grpc::StatusCode code, const QString &error)
{
    if (code != grpc::StatusCode::UNAVAILABLE) {
        return;
    }
    if (state_ == State::Connected) {
        emit disconnected(error);
        setState(State::Disconnected, error);
    }
    scheduleReconnect();
}

void DaemonChannel::scheduleReconnect()
{
    if (!autoReconnect_ || !started_) {
        return;
    }
    if (reconnectTimer_.isActive() || state_ == State::Connecting || state_ == State::Spawning) {
        return;
    }
    reconnectTimer_.start();
}

void DaemonChannel::listRepos()
{
    if (!stub_) {
        emit reposListed(false, {}, tr("the daemon endpoint is not available"));
        return;
    }

    struct Outcome {
        bool ok = false;
        QList<RepoInfo> repos;
        QString error;
        grpc::StatusCode code = grpc::StatusCode::OK;
    };
    auto outcome = std::make_shared<Outcome>();
    auto stub = stub_;
    auto endpoint = endpoint_;

    runAsync(
        [stub, endpoint, outcome] {
            grpc::ClientContext context;
            addAuthMetadata(&context, endpoint);
            setDeadline(&context, kQuickRpcTimeoutMs);

            nipadaemon::ListReposRequest request;
            nipadaemon::ListReposResponse response;
            const grpc::Status result = stub->ListRepos(&context, request, &response);
            outcome->ok = result.ok();
            outcome->code = result.error_code();
            if (result.ok()) {
                for (const auto &repo : response.repos()) {
                    outcome->repos.append(repoFromProto(repo));
                }
            } else {
                outcome->error = rpcErrorText(result);
            }
        },
        [this, outcome] {
            if (!outcome->ok) {
                reportRpcFailure(outcome->code, outcome->error);
            }
            emit reposListed(outcome->ok, outcome->repos, outcome->error);
        });
}

void DaemonChannel::watchRepo(const QString &root)
{
    if (!stub_) {
        emit repoWatched(false, {}, tr("the daemon endpoint is not available"));
        return;
    }

    struct Outcome {
        bool ok = false;
        RepoInfo repo;
        QString error;
        grpc::StatusCode code = grpc::StatusCode::OK;
    };
    auto outcome = std::make_shared<Outcome>();
    auto stub = stub_;
    auto endpoint = endpoint_;

    runAsync(
        [stub, endpoint, outcome, root] {
            grpc::ClientContext context;
            addAuthMetadata(&context, endpoint);
            setDeadline(&context, kQuickRpcTimeoutMs);

            nipadaemon::WatchRepoRequest request;
            request.set_root(root.toStdString());
            nipadaemon::WatchRepoResponse response;
            const grpc::Status result = stub->WatchRepo(&context, request, &response);
            outcome->ok = result.ok();
            outcome->code = result.error_code();
            if (result.ok()) {
                outcome->repo = repoFromProto(response.repo());
            } else {
                outcome->error = rpcErrorText(result);
            }
        },
        [this, outcome] {
            if (!outcome->ok) {
                reportRpcFailure(outcome->code, outcome->error);
            }
            emit repoWatched(outcome->ok, outcome->repo, outcome->error);
        });
}

void DaemonChannel::unwatchRepo(const QString &root)
{
    if (!stub_) {
        emit repoUnwatched(false, root, tr("the daemon endpoint is not available"));
        return;
    }

    struct Outcome {
        bool ok = false;
        QString error;
        grpc::StatusCode code = grpc::StatusCode::OK;
    };
    auto outcome = std::make_shared<Outcome>();
    auto stub = stub_;
    auto endpoint = endpoint_;

    runAsync(
        [stub, endpoint, outcome, root] {
            grpc::ClientContext context;
            addAuthMetadata(&context, endpoint);
            setDeadline(&context, kQuickRpcTimeoutMs);

            nipadaemon::UnwatchRepoRequest request;
            request.set_root(root.toStdString());
            nipadaemon::UnwatchRepoResponse response;
            const grpc::Status result = stub->UnwatchRepo(&context, request, &response);
            outcome->ok = result.ok();
            outcome->code = result.error_code();
            if (!result.ok()) {
                outcome->error = rpcErrorText(result);
            }
        },
        [this, outcome, root] {
            if (!outcome->ok) {
                reportRpcFailure(outcome->code, outcome->error);
            }
            emit repoUnwatched(outcome->ok, root, outcome->error);
        });
}

void DaemonChannel::fetchStatus(const QString &root, bool noCache)
{
    if (!stub_) {
        emit statusFetched(false, root, {}, tr("the daemon endpoint is not available"));
        return;
    }

    struct Outcome {
        bool ok = false;
        QString root;
        StatusSnapshot status;
        QString error;
        grpc::StatusCode code = grpc::StatusCode::OK;
    };
    auto outcome = std::make_shared<Outcome>();
    outcome->root = root;
    auto stub = stub_;
    auto endpoint = endpoint_;

    runAsync(
        [stub, endpoint, outcome, root, noCache] {
            grpc::ClientContext context;
            addAuthMetadata(&context, endpoint);
            setDeadline(&context, kStatusRpcTimeoutMs);

            nipadaemon::StatusRequest request;
            request.set_root(root.toStdString());
            request.set_no_cache(noCache);
            nipadaemon::StatusResponse response;
            const grpc::Status result = stub->Status(&context, request, &response);
            outcome->ok = result.ok();
            outcome->code = result.error_code();
            if (result.ok()) {
                outcome->status = statusFromProto(response);
            } else {
                outcome->error = rpcErrorText(result);
            }
        },
        [this, outcome] {
            if (!outcome->ok) {
                reportRpcFailure(outcome->code, outcome->error);
            }
            emit statusFetched(outcome->ok, outcome->root, outcome->status, outcome->error);
        });
}

void DaemonChannel::shutdownIfOwned()
{
    if (!spawnedByUs_) {
        return;
    }
    spawnPollTimer_.stop();
    reconnectTimer_.stop();

    if (stub_ && state_ == State::Connected) {
        grpc::ClientContext context;
        addAuthMetadata(&context, endpoint_);
        setDeadline(&context, kPingTimeoutMs);
        nipadaemon::ShutdownRequest request;
        nipadaemon::ShutdownResponse response;
        stub_->Shutdown(&context, request, &response);
    }

    // Release ownership before terminating so the finished handler does not
    // report an intentional shutdown as a crash.
    spawnedByUs_ = false;
    if (process_.state() != QProcess::NotRunning) {
        process_.terminate();
        if (!process_.waitForFinished(1500)) {
            process_.kill();
            process_.waitForFinished(500);
        }
    }
}

int DaemonChannel::compareVersions(const QString &a, const QString &b)
{
    QList<int> left = versionSegments(a);
    QList<int> right = versionSegments(b);
    const int length = std::max(left.size(), right.size());
    for (int i = 0; i < length; ++i) {
        const int l = i < left.size() ? left.at(i) : 0;
        const int r = i < right.size() ? right.at(i) : 0;
        if (l != r) {
            return l < r ? -1 : 1;
        }
    }
    return 0;
}
