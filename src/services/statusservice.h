#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

#include "daemon/daemontypes.h"

class DaemonChannel;
class RepositoryService;

/// Owns the status of the active repository: fetches from the daemon's cached
/// status (never rescans) and optionally polls so the pending view stays fresh
/// without the UI ever blocking on a full working-copy scan.
class StatusService : public QObject {
    Q_OBJECT

public:
    StatusService(DaemonChannel *channel, RepositoryService *repository, QObject *parent = nullptr);

    QString root() const;
    StatusSnapshot snapshot() const;

    void setAutoRefreshInterval(int msec); // 0 disables polling
    int autoRefreshInterval() const;
    void setAutoRefreshEnabled(bool enabled);
    bool autoRefreshEnabled() const;

public slots:
    void refresh(bool noCache = false);

signals:
    void statusChanged(const QString &root, const StatusSnapshot &snapshot);
    void refreshFailed(const QString &root, const QString &message);

private:
    DaemonChannel *channel_;
    RepositoryService *repository_;
    QTimer timer_;
    QString root_;
    StatusSnapshot snapshot_;
    bool autoRefreshEnabled_ = true;
};
