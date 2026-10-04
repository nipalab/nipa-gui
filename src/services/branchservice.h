#pragma once

#include <QList>
#include <QObject>

#include "daemon/daemontypes.h"

class DaemonChannel;
class RepositoryService;

/// Branches of the active repository's project: list, create, delete. Switch
/// and merge are streamed operations driven by the window (DaemonOperation).
class BranchService : public QObject {
    Q_OBJECT

public:
    BranchService(DaemonChannel *channel, RepositoryService *repository, QObject *parent = nullptr);

    QList<BranchInfo> branches() const;
    BranchInfo activeBranch() const;
    QString activeBranchName() const;

public slots:
    void refresh();
    void create(const QString &name, const QString &fromBranch);
    void remove(const QString &name);

signals:
    void branchesChanged(const QList<BranchInfo> &branches);
    void errorOccurred(const QString &message);

private:
    DaemonChannel *channel_;
    RepositoryService *repository_;
    QList<BranchInfo> branches_;
};
