#pragma once

#include <QList>
#include <QObject>

#include "daemon/daemontypes.h"

class DaemonChannel;
class RepositoryService;

/// Binary file locks visible to the active repository. Locking uses the active
/// branch as scope (the daemon turns default-branch locks into project-global
/// mainline locks).
class LockService : public QObject {
    Q_OBJECT

public:
    LockService(DaemonChannel *channel, RepositoryService *repository, QObject *parent = nullptr);

    QList<FileLockInfo> locks() const;

    /// Locks whose path covers `path` (equal or directory prefix), used to warn
    /// before staging/submitting files someone else may be editing.
    QList<FileLockInfo> locksForPath(const QString &path) const;

public slots:
    void refresh();
    void lock(const QString &path);
    void unlock(const QString &path);

signals:
    void locksChanged(const QList<FileLockInfo> &locks);
    void errorOccurred(const QString &message);

private:
    DaemonChannel *channel_;
    RepositoryService *repository_;
    QList<FileLockInfo> locks_;
};
