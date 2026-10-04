#pragma once

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <algorithm>
#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <grpcpp/grpcpp.h>

#include "daemon.grpc.pb.h"

/// Script for a fake streamed operation (`Update`, `Push`): optional queueing,
/// `steps` progress events and a terminal result or failure.
struct FakeStreamScript {
    std::string phase = "operation";
    int queuedAhead = 0;
    int steps = 2;
    int stepDelayMs = 0;
    bool fail = false;
    int failureCode = 500;
    std::string failureMessage = "operation failed";
};

/// Script for the fake `Diff` stream: text chunks, optional per-chunk delay
/// (for cancellation tests) and an optional terminal failure.
struct FakeDiffScript {
    std::vector<std::string> chunks;
    int chunkDelayMs = 0;
    bool fail = false;
    int failureCode = 400;
    std::string failureMessage = "diff failed";
};

class FakeDaemonService final : public nipadaemon::NipaDaemon::Service {
public:
    /// Records the token sent on the call and rejects mismatches, mirroring the
    /// daemon's auth interceptor.
    grpc::Status checkAuth(grpc::ServerContext *context)
    {
        for (const auto &entry : context->client_metadata()) {
            const std::string key(entry.first.data(), entry.first.size());
            if (key == "x-nipa-daemon-token") {
                token = std::string(entry.second.data(), entry.second.size());
            }
        }
        if (!requiredToken.empty() && token != requiredToken) {
            return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "invalid token");
        }
        return grpc::Status::OK;
    }

    grpc::Status Ping(grpc::ServerContext *context,
                      const nipadaemon::PingRequest *,
                      nipadaemon::PingResponse *response) override
    {
        if (auto status = checkAuth(context); !status.ok()) {
            return status;
        }
        response->set_version(version);
        response->set_pid(pid);
        return grpc::Status::OK;
    }

    grpc::Status ListRepos(grpc::ServerContext *context,
                           const nipadaemon::ListReposRequest *,
                           nipadaemon::ListReposResponse *response) override
    {
        if (auto status = checkAuth(context); !status.ok()) {
            return status;
        }
        ++listReposCalls;
        for (const auto &repo : repos) {
            response->add_repos()->CopyFrom(repo);
        }
        return grpc::Status::OK;
    }

    grpc::Status WatchRepo(grpc::ServerContext *context,
                           const nipadaemon::WatchRepoRequest *request,
                           nipadaemon::WatchRepoResponse *response) override
    {
        if (auto status = checkAuth(context); !status.ok()) {
            return status;
        }
        watchedRoots.push_back(request->root());
        const auto watched = watchedRepos.find(request->root());
        if (watched != watchedRepos.end()) {
            response->mutable_repo()->CopyFrom(watched->second);
            return grpc::Status::OK;
        }
        auto *repo = response->mutable_repo();
        repo->set_root(request->root());
        const auto known = knownRepos.find(request->root());
        if (known != knownRepos.end()) {
            repo->CopyFrom(known->second);
        }
        return grpc::Status::OK;
    }

    grpc::Status UnwatchRepo(grpc::ServerContext *context,
                             const nipadaemon::UnwatchRepoRequest *request,
                             nipadaemon::UnwatchRepoResponse *) override
    {
        if (auto status = checkAuth(context); !status.ok()) {
            return status;
        }
        unwatchedRoots.push_back(request->root());
        return grpc::Status::OK;
    }

    grpc::Status Status(grpc::ServerContext *context,
                        const nipadaemon::StatusRequest *request,
                        nipadaemon::StatusResponse *response) override
    {
        if (auto status = checkAuth(context); !status.ok()) {
            return status;
        }
        statusRequests.push_back(request->root());
        lastStatusNoCache = request->no_cache();
        const auto known = statuses.find(request->root());
        if (known != statuses.end()) {
            response->CopyFrom(known->second);
        }
        return grpc::Status::OK;
    }

    grpc::Status Stage(grpc::ServerContext *context,
                       const nipadaemon::StageRequest *request,
                       nipadaemon::StatusResponse *response) override
    {
        if (auto status = checkAuth(context); !status.ok()) {
            return status;
        }
        stageRequests.push_back(*request);
        const auto known = statuses.find(request->root());
        if (known != statuses.end()) {
            response->CopyFrom(known->second);
        }
        return grpc::Status::OK;
    }

    grpc::Status ProxyTreeManifest(grpc::ServerContext *context,
                                   const nipadaemon::ProxyTreeManifestRequest *request,
                                   nipadaemon::ProxyTreeManifestResponse *response) override
    {
        if (auto status = checkAuth(context); !status.ok()) {
            return status;
        }
        ++treeRequests;
        lastTreeRoot = request->root();
        lastTreeRequest = request->request();
        const auto known = treeManifests.find(request->root());
        if (known != treeManifests.end()) {
            response->mutable_response()->CopyFrom(known->second);
        }
        return grpc::Status::OK;
    }

    grpc::Status ProxyCommitLog(grpc::ServerContext *context,
                                const nipadaemon::ProxyCommitLogRequest *request,
                                nipadaemon::ProxyCommitLogResponse *response) override
    {
        if (auto status = checkAuth(context); !status.ok()) {
            return status;
        }
        commitLogRequests.push_back(*request);
        const auto known = commitLogs.find(request->root());
        if (known != commitLogs.end()) {
            response->mutable_response()->CopyFrom(known->second);
        }
        return grpc::Status::OK;
    }

    grpc::Status ProxyCommitGet(grpc::ServerContext *context,
                                const nipadaemon::ProxyCommitGetRequest *request,
                                nipadaemon::ProxyCommitGetResponse *response) override
    {
        if (auto status = checkAuth(context); !status.ok()) {
            return status;
        }
        commitGetRequests.push_back(request->request().commit_id());
        const auto known = commitDetails.find(request->request().commit_id());
        if (known != commitDetails.end()) {
            response->mutable_response()->CopyFrom(known->second);
        }
        return grpc::Status::OK;
    }

    grpc::Status Diff(grpc::ServerContext *context,
                      const nipadaemon::DiffRequest *request,
                      grpc::ServerWriter<nipadaemon::DiffEvent> *writer) override
    {
        if (auto status = checkAuth(context); !status.ok()) {
            return status;
        }
        diffRequests.push_back(*request);
        for (const std::string &chunk : diffScript.chunks) {
            if (diffScript.chunkDelayMs > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(diffScript.chunkDelayMs));
            }
            nipadaemon::DiffEvent event;
            event.set_data(chunk);
            if (!writer->Write(event)) {
                return grpc::Status::OK;
            }
        }
        if (diffScript.fail) {
            nipadaemon::DiffEvent event;
            event.mutable_failure()->set_code(diffScript.failureCode);
            event.mutable_failure()->set_message(diffScript.failureMessage);
            writer->Write(event);
        }
        return grpc::Status::OK;
    }

    grpc::Status ProxyListFileLocks(grpc::ServerContext *context,
                                    const nipadaemon::ProxyListFileLocksRequest *,
                                    nipadaemon::ProxyListFileLocksResponse *response) override
    {
        if (auto status = checkAuth(context); !status.ok()) {
            return status;
        }
        ++listLocksRequests;
        for (const auto &lock : locks) {
            response->mutable_response()->add_locks()->CopyFrom(lock);
        }
        return grpc::Status::OK;
    }

    grpc::Status ProxyLockFile(grpc::ServerContext *context,
                               const nipadaemon::ProxyLockFileRequest *request,
                               nipadaemon::ProxyLockFileResponse *response) override
    {
        if (auto status = checkAuth(context); !status.ok()) {
            return status;
        }
        lockRequests.push_back(*request);
        greet::FileLockDetail lock;
        lock.set_id("lock-" + std::to_string(locks.size() + 1));
        lock.set_path(request->request().path());
        lock.set_branch(request->request().branch());
        lock.set_held_by("user-1");
        lock.set_held_by_name("tester");
        locks.push_back(lock);
        response->mutable_response()->mutable_lock()->CopyFrom(lock);
        return grpc::Status::OK;
    }

    grpc::Status ProxyUnlockFile(grpc::ServerContext *context,
                                 const nipadaemon::ProxyUnlockFileRequest *request,
                                 nipadaemon::ProxyUnlockFileResponse *) override
    {
        if (auto status = checkAuth(context); !status.ok()) {
            return status;
        }
        unlockRequests.push_back(*request);
        const std::string path = request->request().path();
        locks.erase(std::remove_if(locks.begin(), locks.end(),
                                   [&path](const greet::FileLockDetail &lock) {
                                       return lock.path() == path;
                                   }),
                    locks.end());
        return grpc::Status::OK;
    }

    grpc::Status Update(grpc::ServerContext *context,
                        const nipadaemon::UpdateRequest *request,
                        grpc::ServerWriter<nipadaemon::OpEvent> *writer) override
    {
        if (auto status = checkAuth(context); !status.ok()) {
            return status;
        }
        updateRequests.push_back(*request);
        return streamOperation(updateScript, writer, [this](nipadaemon::OpEvent *event) {
            auto *result = event->mutable_result()->mutable_sync();
            result->set_branch(updateResultBranch);
            result->set_commit_id(updateResultCommitId);
        });
    }

    grpc::Status Push(grpc::ServerContext *context,
                      const nipadaemon::PushRequest *request,
                      grpc::ServerWriter<nipadaemon::OpEvent> *writer) override
    {
        if (auto status = checkAuth(context); !status.ok()) {
            return status;
        }
        pushRequests.push_back(*request);
        return streamOperation(pushScript, writer, [this](nipadaemon::OpEvent *event) {
            auto *result = event->mutable_result()->mutable_push();
            result->set_commit_id(pushResultCommitId);
            result->set_commit_hash(pushResultCommitHash);
            result->set_tree_hash(pushResultTreeHash);
        });
    }

    std::string requiredToken;
    std::string token;
    std::string version = "9.9.9";
    int pid = 4242;

    std::vector<nipadaemon::RepoInfo> repos;                        // ListRepos result
    std::map<std::string, nipadaemon::RepoInfo> knownRepos;         // WatchRepo fallback
    std::map<std::string, nipadaemon::RepoInfo> watchedRepos;       // per-root WatchRepo result
    std::map<std::string, nipadaemon::StatusResponse> statuses;     // per-root Status result
    std::map<std::string, greet::GetTreeManifestResponse> treeManifests; // per-root tree
    std::map<std::string, greet::GetCommitLogResponse> commitLogs;       // per-root log
    std::map<std::string, greet::GetCommitResponse> commitDetails;       // per-commit detail
    std::vector<std::string> watchedRoots;
    std::vector<std::string> unwatchedRoots;
    std::vector<std::string> statusRequests;
    std::vector<nipadaemon::StageRequest> stageRequests;
    std::vector<nipadaemon::UpdateRequest> updateRequests;
    std::vector<nipadaemon::PushRequest> pushRequests;
    std::vector<nipadaemon::ProxyCommitLogRequest> commitLogRequests;
    std::vector<std::string> commitGetRequests;
    std::vector<greet::FileLockDetail> locks;
    std::vector<nipadaemon::ProxyLockFileRequest> lockRequests;
    std::vector<nipadaemon::ProxyUnlockFileRequest> unlockRequests;
    std::vector<nipadaemon::DiffRequest> diffRequests;
    int listReposCalls = 0;
    int treeRequests = 0;
    int listLocksRequests = 0;
    std::string lastTreeRoot;
    greet::GetTreeManifestRequest lastTreeRequest;
    bool lastStatusNoCache = false;

    FakeStreamScript updateScript{.phase = "update"};
    FakeStreamScript pushScript{.phase = "push"};
    FakeDiffScript diffScript;
    std::string updateResultBranch = "main";
    std::string updateResultCommitId = "updated0001";
    std::string pushResultCommitId = "pushed0001";
    std::string pushResultCommitHash = "hash0001";
    std::string pushResultTreeHash = "tree0001";

private:
    template <typename ResultBuilder>
    grpc::Status streamOperation(const FakeStreamScript &script,
                                 grpc::ServerWriter<nipadaemon::OpEvent> *writer,
                                 ResultBuilder buildResult)
    {
        if (script.queuedAhead > 0) {
            nipadaemon::OpEvent event;
            event.mutable_queued()->set_ahead(script.queuedAhead);
            if (!writer->Write(event)) {
                return grpc::Status::OK;
            }
        }
        {
            nipadaemon::OpEvent event;
            event.mutable_started()->set_phase(script.phase);
            if (!writer->Write(event)) {
                return grpc::Status::OK;
            }
        }
        for (int step = 1; step <= script.steps; ++step) {
            if (script.stepDelayMs > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(script.stepDelayMs));
            }
            nipadaemon::OpEvent event;
            auto *progress = event.mutable_progress();
            progress->set_phase(script.phase);
            progress->set_objects_done(step);
            progress->set_objects_total(script.steps);
            progress->set_bytes_done(step * 100);
            progress->set_bytes_total(script.steps * 100);
            if (!writer->Write(event)) {
                return grpc::Status::OK;
            }
        }
        if (script.fail) {
            nipadaemon::OpEvent event;
            event.mutable_failure()->set_code(script.failureCode);
            event.mutable_failure()->set_message(script.failureMessage);
            writer->Write(event);
            return grpc::Status::OK;
        }
        nipadaemon::OpEvent event;
        buildResult(&event);
        writer->Write(event);
        return grpc::Status::OK;
    }
};

class FakeDaemon {
public:
    FakeDaemon()
    {
        grpc::ServerBuilder builder;
        builder.AddListeningPort("127.0.0.1:0", grpc::InsecureServerCredentials(), &port_);
        builder.RegisterService(&service_);
        server_ = builder.BuildAndStart();
    }

    ~FakeDaemon()
    {
        if (server_) {
            server_->Shutdown();
        }
    }

    int port() const { return port_; }
    FakeDaemonService &service() { return service_; }

private:
    FakeDaemonService service_;
    std::unique_ptr<grpc::Server> server_;
    int port_ = 0;
};

inline QString writeDaemonFile(const QString &path, int port, const QString &token)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return {};
    }
    QJsonObject obj;
    obj.insert(QStringLiteral("pid"), 4242);
    obj.insert(QStringLiteral("port"), port);
    obj.insert(QStringLiteral("token"), token);
    obj.insert(QStringLiteral("version"), QStringLiteral("9.9.9"));
    file.write(QJsonDocument(obj).toJson());
    return path;
}
