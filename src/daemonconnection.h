#pragma once

#include <QString>

struct DaemonStatus {
    QString version;
    qint32 pid = 0;
    QString endpoint;
};

class DaemonConnection {
public:
    DaemonConnection() = default;
    explicit DaemonConnection(const QString &configPath);

    bool ping(DaemonStatus *status, QString *error) const;

private:
    struct Endpoint {
        int pid = 0;
        int port = 0;
        QString token;
        QString version;
    };

    bool loadEndpoint(Endpoint *endpoint, QString *error) const;

    QString configPath_;
};
