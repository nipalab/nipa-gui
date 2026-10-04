#include "daemonchannel.h"

#include <QFutureWatcher>
#include <QStandardPaths>
#include <QTimeZone>

#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <atomic>
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

TreeNodeData treeNodeFromProto(const greet::TreeManifest &manifest, const QString &path)
{
    TreeNodeData node;
    node.name = QString::fromStdString(manifest.path());
    node.path = path;
    node.isDirectory = true;
    for (const auto &file : manifest.files()) {
        TreeNodeData child;
        child.name = QString::fromStdString(file.path());
        child.path = path.isEmpty() ? child.name : path + QLatin1Char('/') + child.name;
        child.isBinary = file.is_binary();
        child.sizeBytes = file.size_bytes();
        child.readOnly = file.mode() == greet::FILE_MODE_READ_ONLY;
        child.isExecutable = file.mode() == greet::FILE_MODE_EXECUTABLE;
        node.children.append(child);
    }
    for (const auto &sub : manifest.sub_trees()) {
        const QString name = QString::fromStdString(sub.path());
        const QString childPath = path.isEmpty() ? name : path + QLatin1Char('/') + name;
        node.children.append(treeNodeFromProto(sub, childPath));
    }
    return node;
}

QDateTime timestampFromProto(const google::protobuf::Timestamp &timestamp)
{
    return QDateTime::fromSecsSinceEpoch(timestamp.seconds(), QTimeZone::UTC);
}

CommitInfo commitLogFromProto(const greet::CommitLogEntry &entry)
{
    CommitInfo commit;
    commit.id = QString::fromStdString(entry.commit_id());
    commit.hash = QString::fromStdString(entry.commit_hash());
    if (entry.has_parent_1_id()) {
        commit.parent1 = QString::fromStdString(entry.parent_1_id());
    }
    if (entry.has_parent_2_id()) {
        commit.parent2 = QString::fromStdString(entry.parent_2_id());
    }
    commit.authorName = QString::fromStdString(entry.author_name());
    commit.authorEmail = QString::fromStdString(entry.author_email());
    commit.message = QString::fromStdString(entry.message());
    commit.createdAt = timestampFromProto(entry.created_at());
    return commit;
}

CommitInfo commitDetailFromProto(const greet::CommitDetail &detail)
{
    CommitInfo commit;
    commit.id = QString::fromStdString(detail.commit_id());
    commit.hash = QString::fromStdString(detail.commit_hash());
    if (detail.has_parent_1_id()) {
        commit.parent1 = QString::fromStdString(detail.parent_1_id());
    }
    if (detail.has_parent_2_id()) {
        commit.parent2 = QString::fromStdString(detail.parent_2_id());
    }
    commit.message = QString::fromStdString(detail.message());
    commit.createdAt = timestampFromProto(detail.created_at());
    return commit;
}

FileLockInfo lockFromProto(const greet::FileLockDetail &lock)
{
    FileLockInfo info;
    info.id = QString::fromStdString(lock.id());
    info.path = QString::fromStdString(lock.path());
    info.branch = QString::fromStdString(lock.branch());
    info.global = lock.global();
    info.heldBy = QString::fromStdString(lock.held_by());
    info.heldByName = QString::fromStdString(lock.held_by_name());
    if (lock.has_merge_request_number()) {
        info.hasMergeRequest = true;
        info.mergeRequestNumber = lock.merge_request_number();
    }
    info.acquiredAt = timestampFromProto(lock.acquired_at());
    return info;
}

BranchInfo branchFromProto(const greet::Branch &branch)
{
    BranchInfo info;
    info.id = QString::fromStdString(branch.id());
    info.name = QString::fromStdString(branch.name());
    if (branch.has_commit_id()) {
        info.commitId = QString::fromStdString(branch.commit_id());
    }
    info.isProtected = branch.is_protected();
    info.isDefault = branch.is_default();
    info.createdAt = timestampFromProto(branch.created_at());
    info.updatedAt = timestampFromProto(branch.updated_at());
    return info;
}

CommitInfo commitWalkFromProto(const greet::CommitWalkEntry &entry)
{
    CommitInfo commit;
    commit.id = QString::fromStdString(entry.commit_id());
    commit.hash = QString::fromStdString(entry.commit_hash());
    if (entry.has_parent_1_id()) {
        commit.parent1 = QString::fromStdString(entry.parent_1_id());
    }
    if (entry.has_parent_2_id()) {
        commit.parent2 = QString::fromStdString(entry.parent_2_id());
    }
    commit.message = QString::fromStdString(entry.message());
    commit.createdAt = timestampFromProto(entry.created_at());
    return commit;
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
    qRegisterMetaType<TreeNodeData>();
    qRegisterMetaType<OpProgressInfo>();
    qRegisterMetaType<OperationResult>();
    qRegisterMetaType<CommitInfo>();
    qRegisterMetaType<QList<CommitInfo>>();
    qRegisterMetaType<FileLockInfo>();
    qRegisterMetaType<QList<FileLockInfo>>();
    qRegisterMetaType<DiffRequestData>();
    qRegisterMetaType<BranchInfo>();
    qRegisterMetaType<QList<BranchInfo>>();

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

void DaemonChannel::fetchTree(const QString &root, const QString &branch, const QStringList &paths)
{
    if (!stub_) {
        emit treeFetched(false, root, branch, {}, tr("the daemon endpoint is not available"));
        return;
    }

    struct Outcome {
        bool ok = false;
        TreeNodeData rootNode;
        QString error;
        grpc::StatusCode code = grpc::StatusCode::OK;
    };
    auto outcome = std::make_shared<Outcome>();
    auto stub = stub_;
    auto endpoint = endpoint_;

    runAsync(
        [stub, endpoint, outcome, root, branch, paths] {
            grpc::ClientContext context;
            addAuthMetadata(&context, endpoint);
            setDeadline(&context, kStatusRpcTimeoutMs);

            nipadaemon::ProxyTreeManifestRequest request;
            request.set_root(root.toStdString());
            auto *inner = request.mutable_request();
            inner->set_branch(branch.toStdString());
            inner->set_recursive(true);
            for (const QString &path : paths) {
                inner->add_paths(path.toStdString());
            }

            nipadaemon::ProxyTreeManifestResponse response;
            const grpc::Status result = stub->ProxyTreeManifest(&context, request, &response);
            outcome->ok = result.ok();
            outcome->code = result.error_code();
            if (result.ok()) {
                outcome->rootNode = treeNodeFromProto(response.response().root_tree(), QString());
            } else {
                outcome->error = rpcErrorText(result);
            }
        },
        [this, outcome, root, branch] {
            if (!outcome->ok) {
                reportRpcFailure(outcome->code, outcome->error);
            }
            emit treeFetched(outcome->ok, root, branch, outcome->rootNode, outcome->error);
        });
}

void DaemonChannel::stage(const QString &root, const QStringList &add, const QStringList &unstage)
{
    if (!stub_) {
        emit staged(false, root, {}, tr("the daemon endpoint is not available"));
        return;
    }

    struct Outcome {
        bool ok = false;
        StatusSnapshot status;
        QString error;
        grpc::StatusCode code = grpc::StatusCode::OK;
    };
    auto outcome = std::make_shared<Outcome>();
    auto stub = stub_;
    auto endpoint = endpoint_;

    runAsync(
        [stub, endpoint, outcome, root, add, unstage] {
            grpc::ClientContext context;
            addAuthMetadata(&context, endpoint);
            setDeadline(&context, kStatusRpcTimeoutMs);

            nipadaemon::StageRequest request;
            request.set_root(root.toStdString());
            for (const QString &path : add) {
                request.add_add(path.toStdString());
            }
            for (const QString &path : unstage) {
                request.add_unstage(path.toStdString());
            }

            nipadaemon::StatusResponse response;
            const grpc::Status result = stub->Stage(&context, request, &response);
            outcome->ok = result.ok();
            outcome->code = result.error_code();
            if (result.ok()) {
                outcome->status = statusFromProto(response);
            } else {
                outcome->error = rpcErrorText(result);
            }
        },
        [this, outcome, root] {
            if (!outcome->ok) {
                reportRpcFailure(outcome->code, outcome->error);
            }
            emit staged(outcome->ok, root, outcome->status, outcome->error);
        });
}

DaemonOperation *DaemonChannel::startOperation(
    DaemonOperation::Kind kind,
    std::function<std::unique_ptr<OpReader>(grpc::ClientContext *)> startStream)
{
    auto *operation = new DaemonOperation(kind, this);
    if (!stub_) {
        queueOperationFailure(operation, 0, tr("the daemon endpoint is not available"));
        return operation;
    }

    auto context = std::make_shared<grpc::ClientContext>();
    addAuthMetadata(context.get(), endpoint_);
    operation->setCancelContext(context);

    auto terminal = std::make_shared<std::atomic<bool>>(false);
    operation->setWork(QtConcurrent::run(
        [operation, context, startStream = std::move(startStream), terminal] {
            std::unique_ptr<OpReader> reader = startStream(context.get());
            nipadaemon::OpEvent event;
            while (reader->Read(&event)) {
                if (event.has_result() || event.has_failure()) {
                    terminal->store(true);
                }
                const nipadaemon::OpEvent copy = event;
                QMetaObject::invokeMethod(
                    operation, [operation, copy] { operation->handleEvent(copy); }, Qt::QueuedConnection);
            }
            const grpc::Status status = reader->Finish();
            if (!terminal->load()) {
                const bool wasCancelled = operation->isCancelled();
                const int code = wasCancelled ? 499 : static_cast<int>(status.error_code());
                const QString message =
                    wasCancelled ? QObject::tr("cancelled")
                                 : (status.ok() ? QObject::tr("the operation ended without a result")
                                                : QString::fromStdString(status.error_message()));
                QMetaObject::invokeMethod(
                    operation,
                    [operation, code, message] {
                        nipadaemon::OpEvent failure;
                        failure.mutable_failure()->set_code(code);
                        failure.mutable_failure()->set_message(message.toStdString());
                        operation->handleEvent(failure);
                    },
                    Qt::QueuedConnection);
            }
        }));
    return operation;
}

void DaemonChannel::queueOperationFailure(DaemonOperation *operation, int code, const QString &message)
{
    QMetaObject::invokeMethod(
        operation,
        [operation, code, message] {
            nipadaemon::OpEvent failure;
            failure.mutable_failure()->set_code(code);
            failure.mutable_failure()->set_message(message.toStdString());
            operation->handleEvent(failure);
        },
        Qt::QueuedConnection);
}

DaemonOperation *DaemonChannel::update(const QString &root)
{
    auto stub = stub_;
    nipadaemon::UpdateRequest request;
    request.set_root(root.toStdString());
    return startOperation(DaemonOperation::Kind::Update,
                          [stub, request](grpc::ClientContext *context) -> std::unique_ptr<OpReader> {
                              return std::unique_ptr<OpReader>(stub->Update(context, request));
                          });
}

DaemonOperation *DaemonChannel::push(const QString &root, const QString &message)
{
    auto stub = stub_;
    nipadaemon::PushRequest request;
    request.set_root(root.toStdString());
    request.set_message(message.toStdString());
    return startOperation(DaemonOperation::Kind::Push,
                          [stub, request](grpc::ClientContext *context) -> std::unique_ptr<OpReader> {
                              return std::unique_ptr<OpReader>(stub->Push(context, request));
                          });
}

void DaemonChannel::fetchCommitLog(const QString &root,
                                   const QString &branch,
                                   const QString &startCommitId,
                                   int limit)
{
    if (!stub_) {
        emit commitLogFetched(false, root, branch, {}, tr("the daemon endpoint is not available"));
        return;
    }

    struct Outcome {
        bool ok = false;
        QList<CommitInfo> commits;
        QString error;
        grpc::StatusCode code = grpc::StatusCode::OK;
    };
    auto outcome = std::make_shared<Outcome>();
    auto stub = stub_;
    auto endpoint = endpoint_;

    runAsync(
        [stub, endpoint, outcome, root, branch, startCommitId, limit] {
            grpc::ClientContext context;
            addAuthMetadata(&context, endpoint);
            setDeadline(&context, kStatusRpcTimeoutMs);

            nipadaemon::ProxyCommitLogRequest request;
            request.set_root(root.toStdString());
            auto *inner = request.mutable_request();
            inner->set_branch(branch.toStdString());
            inner->set_limit(limit);
            if (!startCommitId.isEmpty()) {
                inner->set_start_commit_id(startCommitId.toStdString());
            }

            nipadaemon::ProxyCommitLogResponse response;
            const grpc::Status result = stub->ProxyCommitLog(&context, request, &response);
            outcome->ok = result.ok();
            outcome->code = result.error_code();
            if (result.ok()) {
                for (const auto &entry : response.response().commits()) {
                    outcome->commits.append(commitLogFromProto(entry));
                }
            } else {
                outcome->error = rpcErrorText(result);
            }
        },
        [this, outcome, root, branch] {
            if (!outcome->ok) {
                reportRpcFailure(outcome->code, outcome->error);
            }
            emit commitLogFetched(outcome->ok, root, branch, outcome->commits, outcome->error);
        });
}

void DaemonChannel::fetchCommit(const QString &root, const QString &commitId)
{
    if (!stub_) {
        emit commitFetched(false, root, {}, {}, tr("the daemon endpoint is not available"));
        return;
    }

    struct Outcome {
        bool ok = false;
        CommitInfo commit;
        TreeNodeData tree;
        QString error;
        grpc::StatusCode code = grpc::StatusCode::OK;
    };
    auto outcome = std::make_shared<Outcome>();
    auto stub = stub_;
    auto endpoint = endpoint_;

    runAsync(
        [stub, endpoint, outcome, root, commitId] {
            grpc::ClientContext context;
            addAuthMetadata(&context, endpoint);
            setDeadline(&context, kStatusRpcTimeoutMs);

            nipadaemon::ProxyCommitGetRequest request;
            request.set_root(root.toStdString());
            request.mutable_request()->set_commit_id(commitId.toStdString());

            nipadaemon::ProxyCommitGetResponse response;
            const grpc::Status result = stub->ProxyCommitGet(&context, request, &response);
            outcome->ok = result.ok();
            outcome->code = result.error_code();
            if (result.ok()) {
                outcome->commit = commitDetailFromProto(response.response().commit());
                outcome->tree = treeNodeFromProto(response.response().root_tree(), QString());
            } else {
                outcome->error = rpcErrorText(result);
            }
        },
        [this, outcome, root] {
            if (!outcome->ok) {
                reportRpcFailure(outcome->code, outcome->error);
            }
            emit commitFetched(outcome->ok, root, outcome->commit, outcome->tree, outcome->error);
        });
}

void DaemonChannel::fetchLocks(const QString &root)
{
    if (!stub_) {
        emit locksFetched(false, root, {}, tr("the daemon endpoint is not available"));
        return;
    }

    struct Outcome {
        bool ok = false;
        QList<FileLockInfo> locks;
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

            nipadaemon::ProxyListFileLocksRequest request;
            request.set_root(root.toStdString());
            request.mutable_request();

            nipadaemon::ProxyListFileLocksResponse response;
            const grpc::Status result = stub->ProxyListFileLocks(&context, request, &response);
            outcome->ok = result.ok();
            outcome->code = result.error_code();
            if (result.ok()) {
                for (const auto &lock : response.response().locks()) {
                    outcome->locks.append(lockFromProto(lock));
                }
            } else {
                outcome->error = rpcErrorText(result);
            }
        },
        [this, outcome, root] {
            if (!outcome->ok) {
                reportRpcFailure(outcome->code, outcome->error);
            }
            emit locksFetched(outcome->ok, root, outcome->locks, outcome->error);
        });
}

void DaemonChannel::lockFile(const QString &root, const QString &path, const QString &branch)
{
    if (!stub_) {
        emit fileLocked(false, root, {}, tr("the daemon endpoint is not available"));
        return;
    }

    struct Outcome {
        bool ok = false;
        FileLockInfo lock;
        QString error;
        grpc::StatusCode code = grpc::StatusCode::OK;
    };
    auto outcome = std::make_shared<Outcome>();
    auto stub = stub_;
    auto endpoint = endpoint_;

    runAsync(
        [stub, endpoint, outcome, root, path, branch] {
            grpc::ClientContext context;
            addAuthMetadata(&context, endpoint);
            setDeadline(&context, kQuickRpcTimeoutMs);

            nipadaemon::ProxyLockFileRequest request;
            request.set_root(root.toStdString());
            auto *inner = request.mutable_request();
            inner->set_path(path.toStdString());
            inner->set_branch(branch.toStdString());

            nipadaemon::ProxyLockFileResponse response;
            const grpc::Status result = stub->ProxyLockFile(&context, request, &response);
            outcome->ok = result.ok();
            outcome->code = result.error_code();
            if (result.ok()) {
                outcome->lock = lockFromProto(response.response().lock());
            } else {
                outcome->error = rpcErrorText(result);
            }
        },
        [this, outcome, root] {
            if (!outcome->ok) {
                reportRpcFailure(outcome->code, outcome->error);
            }
            emit fileLocked(outcome->ok, root, outcome->lock, outcome->error);
        });
}

void DaemonChannel::unlockFile(const QString &root, const QString &path, const QString &branch)
{
    if (!stub_) {
        emit fileUnlocked(false, root, path, tr("the daemon endpoint is not available"));
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
        [stub, endpoint, outcome, root, path, branch] {
            grpc::ClientContext context;
            addAuthMetadata(&context, endpoint);
            setDeadline(&context, kQuickRpcTimeoutMs);

            nipadaemon::ProxyUnlockFileRequest request;
            request.set_root(root.toStdString());
            auto *inner = request.mutable_request();
            inner->set_path(path.toStdString());
            inner->set_branch(branch.toStdString());

            nipadaemon::ProxyUnlockFileResponse response;
            const grpc::Status result = stub->ProxyUnlockFile(&context, request, &response);
            outcome->ok = result.ok();
            outcome->code = result.error_code();
            if (!result.ok()) {
                outcome->error = rpcErrorText(result);
            }
        },
        [this, outcome, root, path] {
            if (!outcome->ok) {
                reportRpcFailure(outcome->code, outcome->error);
            }
            emit fileUnlocked(outcome->ok, root, path, outcome->error);
        });
}

void DaemonChannel::fetchBranches(const QString &root)
{
    if (!stub_) {
        emit branchesFetched(false, root, {}, tr("the daemon endpoint is not available"));
        return;
    }

    struct Outcome {
        bool ok = false;
        QList<BranchInfo> branches;
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

            nipadaemon::ProxyBranchListRequest request;
            request.set_root(root.toStdString());
            request.mutable_request();

            nipadaemon::ProxyBranchListResponse response;
            const grpc::Status result = stub->ProxyBranchList(&context, request, &response);
            outcome->ok = result.ok();
            outcome->code = result.error_code();
            if (result.ok()) {
                for (const auto &branch : response.response().branches()) {
                    outcome->branches.append(branchFromProto(branch));
                }
            } else {
                outcome->error = rpcErrorText(result);
            }
        },
        [this, outcome, root] {
            if (!outcome->ok) {
                reportRpcFailure(outcome->code, outcome->error);
            }
            emit branchesFetched(outcome->ok, root, outcome->branches, outcome->error);
        });
}

void DaemonChannel::createBranch(const QString &root, const QString &name, const QString &fromBranch)
{
    if (!stub_) {
        emit branchCreated(false, root, {}, tr("the daemon endpoint is not available"));
        return;
    }

    struct Outcome {
        bool ok = false;
        BranchInfo branch;
        QString error;
        grpc::StatusCode code = grpc::StatusCode::OK;
    };
    auto outcome = std::make_shared<Outcome>();
    auto stub = stub_;
    auto endpoint = endpoint_;

    runAsync(
        [stub, endpoint, outcome, root, name, fromBranch] {
            grpc::ClientContext context;
            addAuthMetadata(&context, endpoint);
            setDeadline(&context, kQuickRpcTimeoutMs);

            nipadaemon::ProxyBranchCreateRequest request;
            request.set_root(root.toStdString());
            auto *inner = request.mutable_request();
            inner->set_name(name.toStdString());
            inner->set_from_branch(fromBranch.toStdString());

            nipadaemon::ProxyBranchCreateResponse response;
            const grpc::Status result = stub->ProxyBranchCreate(&context, request, &response);
            outcome->ok = result.ok();
            outcome->code = result.error_code();
            if (result.ok()) {
                outcome->branch = branchFromProto(response.response().branch());
            } else {
                outcome->error = rpcErrorText(result);
            }
        },
        [this, outcome, root] {
            if (!outcome->ok) {
                reportRpcFailure(outcome->code, outcome->error);
            }
            emit branchCreated(outcome->ok, root, outcome->branch, outcome->error);
        });
}

void DaemonChannel::deleteBranch(const QString &root, const QString &name)
{
    if (!stub_) {
        emit branchDeleted(false, root, name, tr("the daemon endpoint is not available"));
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
        [stub, endpoint, outcome, root, name] {
            grpc::ClientContext context;
            addAuthMetadata(&context, endpoint);
            setDeadline(&context, kQuickRpcTimeoutMs);

            nipadaemon::ProxyBranchDeleteRequest request;
            request.set_root(root.toStdString());
            request.mutable_request()->set_name(name.toStdString());

            nipadaemon::ProxyBranchDeleteResponse response;
            const grpc::Status result = stub->ProxyBranchDelete(&context, request, &response);
            outcome->ok = result.ok();
            outcome->code = result.error_code();
            if (!result.ok()) {
                outcome->error = rpcErrorText(result);
            }
        },
        [this, outcome, root, name] {
            if (!outcome->ok) {
                reportRpcFailure(outcome->code, outcome->error);
            }
            emit branchDeleted(outcome->ok, root, name, outcome->error);
        });
}

void DaemonChannel::fetchCommitWalk(const QString &root, const QString &startCommitId, int limit)
{
    if (!stub_) {
        emit commitWalkFetched(false, root, {}, tr("the daemon endpoint is not available"));
        return;
    }

    struct Outcome {
        bool ok = false;
        QList<CommitInfo> commits;
        QString error;
        grpc::StatusCode code = grpc::StatusCode::OK;
    };
    auto outcome = std::make_shared<Outcome>();
    auto stub = stub_;
    auto endpoint = endpoint_;

    runAsync(
        [stub, endpoint, outcome, root, startCommitId, limit] {
            grpc::ClientContext context;
            addAuthMetadata(&context, endpoint);
            setDeadline(&context, kStatusRpcTimeoutMs);

            nipadaemon::ProxyCommitWalkRequest request;
            request.set_root(root.toStdString());
            auto *inner = request.mutable_request();
            inner->set_start_commit_id(startCommitId.toStdString());
            inner->set_limit(limit);

            nipadaemon::ProxyCommitWalkResponse response;
            const grpc::Status result = stub->ProxyCommitWalk(&context, request, &response);
            outcome->ok = result.ok();
            outcome->code = result.error_code();
            if (result.ok()) {
                for (const auto &entry : response.response().commits()) {
                    outcome->commits.append(commitWalkFromProto(entry));
                }
            } else {
                outcome->error = rpcErrorText(result);
            }
        },
        [this, outcome, root] {
            if (!outcome->ok) {
                reportRpcFailure(outcome->code, outcome->error);
            }
            emit commitWalkFetched(outcome->ok, root, outcome->commits, outcome->error);
        });
}

DaemonOperation *DaemonChannel::switchBranch(const QString &root, const QString &branch)
{
    auto stub = stub_;
    nipadaemon::SwitchRequest request;
    request.set_root(root.toStdString());
    request.set_branch(branch.toStdString());
    return startOperation(DaemonOperation::Kind::Switch,
                          [stub, request](grpc::ClientContext *context) -> std::unique_ptr<OpReader> {
                              return std::unique_ptr<OpReader>(stub->Switch(context, request));
                          });
}

DaemonOperation *DaemonChannel::merge(const QString &root,
                                      const QString &sourceBranch,
                                      bool abort,
                                      bool ffOnly,
                                      bool noFf,
                                      const QString &message)
{
    auto stub = stub_;
    nipadaemon::MergeOpRequest request;
    request.set_root(root.toStdString());
    request.set_source_branch(sourceBranch.toStdString());
    request.set_abort(abort);
    request.set_ff_only(ffOnly);
    request.set_no_ff(noFf);
    request.set_message(message.toStdString());
    return startOperation(DaemonOperation::Kind::Merge,
                          [stub, request](grpc::ClientContext *context) -> std::unique_ptr<OpReader> {
                              return std::unique_ptr<OpReader>(stub->Merge(context, request));
                          });
}

DaemonDiff *DaemonChannel::diff(const QString &root, const DiffRequestData &requestData)
{
    auto *diffStream = new DaemonDiff(this);
    if (!stub_) {
        const QString message = tr("the daemon endpoint is not available");
        QMetaObject::invokeMethod(
            diffStream,
            [diffStream, message] {
                nipadaemon::DiffEvent failure;
                failure.mutable_failure()->set_code(0);
                failure.mutable_failure()->set_message(message.toStdString());
                diffStream->handleEvent(failure);
            },
            Qt::QueuedConnection);
        return diffStream;
    }

    auto context = std::make_shared<grpc::ClientContext>();
    addAuthMetadata(context.get(), endpoint_);
    diffStream->setCancelContext(context);

    auto stub = stub_;
    auto request = std::make_shared<nipadaemon::DiffRequest>();
    request->set_root(root.toStdString());
    for (const QString &revision : requestData.revisions) {
        request->add_revisions(revision.toStdString());
    }
    for (const QString &path : requestData.paths) {
        request->add_paths(path.toStdString());
    }
    request->set_staged(requestData.staged);
    request->set_no_cache(requestData.noCache);
    request->set_merge_base(requestData.mergeBase);
    request->set_format(requestData.format.toStdString());
    request->set_ignore_all_space(requestData.ignoreAllSpace);
    request->set_ignore_space_change(requestData.ignoreSpaceChange);
    request->set_context(requestData.context);

    auto terminal = std::make_shared<std::atomic<bool>>(false);
    diffStream->setWork(QtConcurrent::run([diffStream, stub, context, request, terminal] {
        std::unique_ptr<grpc::ClientReader<nipadaemon::DiffEvent>> reader =
            stub->Diff(context.get(), *request);
        nipadaemon::DiffEvent event;
        while (reader->Read(&event)) {
            if (event.has_failure()) {
                terminal->store(true);
            }
            const nipadaemon::DiffEvent copy = event;
            QMetaObject::invokeMethod(
                diffStream, [diffStream, copy] { diffStream->handleEvent(copy); }, Qt::QueuedConnection);
        }
        const grpc::Status status = reader->Finish();
        if (terminal->load()) {
            return;
        }
        if (diffStream->isCancelled()) {
            QMetaObject::invokeMethod(
                diffStream,
                [diffStream] {
                    nipadaemon::DiffEvent failure;
                    failure.mutable_failure()->set_code(499);
                    diffStream->handleEvent(failure);
                },
                Qt::QueuedConnection);
        } else if (!status.ok()) {
            const QString message = QString::fromStdString(status.error_message());
            QMetaObject::invokeMethod(
                diffStream,
                [diffStream, message] {
                    nipadaemon::DiffEvent failure;
                    failure.mutable_failure()->set_code(500);
                    failure.mutable_failure()->set_message(message.toStdString());
                    diffStream->handleEvent(failure);
                },
                Qt::QueuedConnection);
        } else {
            QMetaObject::invokeMethod(
                diffStream, [diffStream] { diffStream->reportFinished(); }, Qt::QueuedConnection);
        }
    }));
    return diffStream;
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
