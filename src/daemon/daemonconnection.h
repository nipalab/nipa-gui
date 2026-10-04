#pragma once

#include <QString>

/// The loopback endpoint published by `nipa serve` in the daemon discovery
/// file (`~/.config/nipa/daemon.json`, 0600).
struct DaemonEndpoint {
    int pid = 0;
    int port = 0;
    QString token;
    QString version;

    bool isValid() const { return port > 0 && !token.isEmpty(); }
    QString address() const { return QStringLiteral("127.0.0.1:") + QString::number(port); }
};

/// Reads the daemon discovery file. All transport lives in DaemonChannel; this
/// class only owns discovery so the endpoint contract stays unit-testable.
class DaemonConnection {
public:
    DaemonConnection() = default;
    explicit DaemonConnection(const QString &configPath);

    /// Platform default: mirrors Go's os.UserConfigDir().
    static QString defaultConfigPath();

    /// Loads and validates the discovery file. Exposed so tests can point at a
    /// temporary path; `configPath()` returns the resolved path.
    bool load(DaemonEndpoint *endpoint, QString *error) const;

    QString configPath() const;

private:
    QString configPath_;
};
