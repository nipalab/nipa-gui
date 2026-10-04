#pragma once

#include <QList>
#include <QObject>

#include "daemon/daemontypes.h"

class DaemonChannel;

/// Tracks the daemon's registry of working copies and the workspace the UI has
/// open. Selection is separate from transport so status polling always targets
/// an explicitly watched root.
class WorkspaceService : public QObject {
    Q_OBJECT

public:
    explicit WorkspaceService(DaemonChannel *channel, QObject *parent = nullptr);

    QList<RepoInfo> repos() const;
    RepoInfo activeRepo() const;
    QString activeRoot() const;
    bool hasActiveRepo() const;

public slots:
    void refresh();
    void open(const QString &root);
    void close();

signals:
    void reposChanged(const QList<RepoInfo> &repos);
    void activeRepoChanged(const RepoInfo &repo); // empty root when closed
    void errorOccurred(const QString &message);

private:
    DaemonChannel *channel_;
    QList<RepoInfo> repos_;
    RepoInfo activeRepo_;
};
