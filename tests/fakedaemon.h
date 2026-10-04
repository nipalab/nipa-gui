#pragma once

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <grpcpp/grpcpp.h>

#include "daemon.grpc.pb.h"

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

    std::string requiredToken;
    std::string token;
    std::string version = "9.9.9";
    int pid = 4242;

    std::vector<nipadaemon::RepoInfo> repos;                        // ListRepos result
    std::map<std::string, nipadaemon::RepoInfo> knownRepos;         // WatchRepo fallback
    std::map<std::string, nipadaemon::RepoInfo> watchedRepos;       // per-root WatchRepo result
    std::map<std::string, nipadaemon::StatusResponse> statuses;     // per-root Status result
    std::vector<std::string> watchedRoots;
    std::vector<std::string> unwatchedRoots;
    std::vector<std::string> statusRequests;
    int listReposCalls = 0;
    bool lastStatusNoCache = false;
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
