#pragma once

#include <QDateTime>
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

/// One node of the repository tree at the active branch head. The daemon
/// serves the manifest recursively (scoped by the clone's sparse prefixes), so
/// the whole tree is fetched once and children are materialized on demand by
/// the model.
struct TreeNodeData {
    QString name;
    QString path; // repository-relative
    bool isDirectory = false;
    bool isBinary = false;
    bool isExecutable = false;
    bool readOnly = false;
    qint64 sizeBytes = 0;
    QList<TreeNodeData> children;
};

/// Progress of a long operation (`Update`, `Push`, ...), from `OpProgress`.
struct OpProgressInfo {
    QString phase;
    qint64 objectsDone = 0;
    qint64 objectsTotal = 0;
    qint64 bytesDone = 0;
    qint64 bytesTotal = 0;
};

struct SyncResultInfo {
    QString branch;
    QString commitId;
};

struct PushResultInfo {
    QString commitId;
    QString commitHash;
    QString treeHash;
};

struct MergeResultInfo {
    bool upToDate = false;
    bool fastForwarded = false;
    bool mergeCommitted = false;
    bool aborted = false;
    QStringList conflicts;
};

struct RevertResultInfo {
    bool committed = false;
    bool noChange = false;
    bool skipped = false;
    bool aborted = false;
    QStringList conflicts;
};

/// Terminal outcome of a streamed operation (`OpResult`).
struct OperationResult {
    enum Kind {
        Sync, // Update / Switch
        Push,
        Merge,
        Revert,
    };

    Kind kind = Sync;
    SyncResultInfo sync;
    PushResultInfo push;
    MergeResultInfo merge;
    RevertResultInfo revert;
};

Q_DECLARE_METATYPE(TreeNodeData)
Q_DECLARE_METATYPE(OpProgressInfo)
Q_DECLARE_METATYPE(OperationResult)

/// One commit log entry (`ProxyCommitLog`).
struct CommitInfo {
    QString id;   // base36 snow ID
    QString hash; // hex content hash
    QString parent1;
    QString parent2; // set for merge commits
    QString authorName;
    QString authorEmail;
    QString message;
    QDateTime createdAt;
};

/// One binary file lock (`ProxyListFileLocks`). Locks on the default branch are
/// project-global; `branch` is empty for mainline locks.
struct FileLockInfo {
    QString id;
    QString path;
    QString branch;
    bool global = false;
    QString heldBy;
    QString heldByName;
    bool hasMergeRequest = false;
    qint64 mergeRequestNumber = 0;
    QDateTime acquiredAt;
};

/// Parameters for the streaming `Diff` RPC. Empty `revisions` compares the
/// working copy against the last synced snapshot; one revision compares it
/// against the working copy; two compare revisions against each other.
struct DiffRequestData {
    QStringList revisions;
    QStringList paths;
    bool staged = false;
    bool noCache = false;
    bool mergeBase = false;
    QString format = QStringLiteral("patch"); // patch, stat, name_only, name_status
    bool ignoreAllSpace = false;
    bool ignoreSpaceChange = false;
    int context = 0;
};

Q_DECLARE_METATYPE(CommitInfo)
Q_DECLARE_METATYPE(FileLockInfo)
Q_DECLARE_METATYPE(DiffRequestData)
