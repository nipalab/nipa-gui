#pragma once

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>

#include "daemon.grpc.pb.h"

class FakeDaemonService final : public nipadaemon::NipaDaemon::Service {
public:
    grpc::Status Ping(grpc::ServerContext *context,
                      const nipadaemon::PingRequest *,
                      nipadaemon::PingResponse *response) override
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
        response->set_version(version);
        response->set_pid(pid);
        return grpc::Status::OK;
    }

    std::string requiredToken;
    std::string token;
    std::string version = "9.9.9";
    int pid = 4242;
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
