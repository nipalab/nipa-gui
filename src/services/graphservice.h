#pragma once

#include <QList>
#include <QObject>

#include "daemon/daemontypes.h"

class BranchService;
class DaemonChannel;
class RepositoryService;

/// Revision graph data: a newest-first walk of both parents from the active
/// branch head (`ProxyCommitWalk`). Layout (lanes) is computed by
/// RevisionGraphModel.
class GraphService : public QObject {
    Q_OBJECT

public:
    GraphService(DaemonChannel *channel,
                 RepositoryService *repository,
                 BranchService *branches,
                 QObject *parent = nullptr);

    QList<CommitInfo> commits() const;

    static constexpr int kGraphLimit = 200;

public slots:
    void refresh();
    void clear();

signals:
    void graphChanged(const QList<CommitInfo> &commits);
    void errorOccurred(const QString &message);

private:
    DaemonChannel *channel_;
    RepositoryService *repository_;
    BranchService *branches_;
    QList<CommitInfo> commits_;
};
