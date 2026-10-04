#pragma once

#include <QMetaType>
#include <QString>
#include <QStringList>

/// Version, pid and endpoint reported by the daemon `Ping` RPC.
struct DaemonStatus {
    QString version;
    qint32 pid = 0;
    QString endpoint;
};

/// A working copy registered with the daemon (`ListRepos` / `WatchRepo`).
struct RepoInfo {
    QString root;
    QString url;
    QString branch;
    QStringList sparse;
};

/// The daemon's cached working-copy status; `headKind`/`headName` are set when
/// HEAD is detached (e.g. after `nipa switch --tag`).
struct StatusSnapshot {
    QStringList staged;
    QStringList deleted;
    QStringList modified;
    QStringList untracked;
    QStringList missing;
    QStringList conflicts;
    QString branch;
    QString headKind;
    QString headName;

    bool detached() const { return !headKind.isEmpty(); }
    int total() const
    {
        return staged.size() + deleted.size() + modified.size() + untracked.size() + missing.size()
               + conflicts.size();
    }
};

Q_DECLARE_METATYPE(DaemonStatus)
Q_DECLARE_METATYPE(RepoInfo)
Q_DECLARE_METATYPE(StatusSnapshot)
