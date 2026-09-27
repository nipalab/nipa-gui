#include "daemonconnection.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QObject>

#include <chrono>
#include <string>

#include <grpcpp/grpcpp.h>

#include "daemon.grpc.pb.h"

namespace {

// Mirrors Go's os.UserConfigDir(): the daemon's discovery file must be found on
// every platform the CLI supports.
QString daemonConfigPath()
{
#ifdef Q_OS_WIN
    const QString appData = qEnvironmentVariable("AppData");
    if (!appData.isEmpty()) {
        return appData + QStringLiteral("/nipa/daemon.json");
    }
    return QDir::homePath() + QStringLiteral("/AppData/Roaming/nipa/daemon.json");
#elif defined(Q_OS_MACOS)
    return QDir::homePath() + QStringLiteral("/Library/Application Support/nipa/daemon.json");
#else
    const QString xdg = qEnvironmentVariable("XDG_CONFIG_HOME");
    if (!xdg.isEmpty()) {
        return xdg + QStringLiteral("/nipa/daemon.json");
    }
    return QDir::homePath() + QStringLiteral("/.config/nipa/daemon.json");
#endif
}

} // namespace

DaemonConnection::DaemonConnection(const QString &configPath)
    : configPath_(configPath)
{
}

bool DaemonConnection::loadEndpoint(Endpoint *endpoint, QString *error) const
{
    const QString path = configPath_.isEmpty() ? daemonConfigPath() : configPath_;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QObject::tr("cannot read %1 (%2) - is `nipa serve` running?").arg(path, file.errorString());
        return false;
    }

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        *error = QObject::tr("invalid daemon record in %1: %2").arg(path, parseError.errorString());
        return false;
    }

    const QJsonObject obj = doc.object();
    endpoint->pid = obj.value(QStringLiteral("pid")).toInt();
    endpoint->port = obj.value(QStringLiteral("port")).toInt();
    endpoint->token = obj.value(QStringLiteral("token")).toString();
    endpoint->version = obj.value(QStringLiteral("version")).toString();
    if (endpoint->port <= 0 || endpoint->token.isEmpty()) {
        *error = QObject::tr("daemon record in %1 is incomplete").arg(path);
        return false;
    }
    return true;
}

bool DaemonConnection::ping(DaemonStatus *status, QString *error) const
{
    Endpoint endpoint;
    if (!loadEndpoint(&endpoint, error)) {
        return false;
    }

    const std::string target = "127.0.0.1:" + std::to_string(endpoint.port);
    auto channel = grpc::CreateChannel(target, grpc::InsecureChannelCredentials());
    if (!channel->WaitForConnected(std::chrono::system_clock::now() + std::chrono::seconds(3))) {
        *error = QObject::tr("the daemon is not answering on %1").arg(QString::fromStdString(target));
        return false;
    }

    grpc::ClientContext context;
    context.AddMetadata("x-nipa-daemon-token", endpoint.token.toStdString());
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(3));

    nipadaemon::PingRequest request;
    nipadaemon::PingResponse response;
    auto stub = nipadaemon::NipaDaemon::NewStub(channel);
    const grpc::Status result = stub->Ping(&context, request, &response);
    if (!result.ok()) {
        *error = QObject::tr("the daemon rejected the request: %1")
                     .arg(QString::fromStdString(result.error_message()));
        return false;
    }

    status->version = QString::fromStdString(response.version());
    status->pid = response.pid();
    status->endpoint = QString::fromStdString(target);
    return true;
}
