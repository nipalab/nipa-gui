#include "daemonconnection.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QObject>

namespace {

QString resolveConfigPath(const QString &configPath)
{
    return configPath.isEmpty() ? DaemonConnection::defaultConfigPath() : configPath;
}

} // namespace

DaemonConnection::DaemonConnection(const QString &configPath)
    : configPath_(configPath)
{
}

QString DaemonConnection::defaultConfigPath()
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

QString DaemonConnection::configPath() const
{
    return resolveConfigPath(configPath_);
}

bool DaemonConnection::load(DaemonEndpoint *endpoint, QString *error) const
{
    const QString path = configPath();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QObject::tr("cannot read %1 (%2) - is `nipa serve` running?")
                     .arg(path, file.errorString());
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
    if (!endpoint->isValid()) {
        *error = QObject::tr("daemon record in %1 is incomplete").arg(path);
        return false;
    }
    return true;
}
