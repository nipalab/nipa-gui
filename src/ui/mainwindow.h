#pragma once

#include <QMainWindow>
#include <QModelIndex>
#include <QString>

#include "daemon/daemonchannel.h"
#include "daemon/daemondiff.h"
#include "daemon/daemontypes.h"

class QAction;
class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTableView;
class QTabWidget;
class QTreeView;

class CommitLogModel;
class BranchModel;
class BranchService;
class DaemonOperation;
class DiffView;
class GraphService;
class HistoryService;
class LockService;
class LocksModel;
class RepositoryService;
class RepositoryTreeModel;
class RevisionGraphModel;
class StatusModel;
class StatusService;

/// P4V-style shell: repositories and tree docks on the left, pending changes
/// docked at the bottom, and the History/Diff/Locks editors as central tabs.
/// The window is a pure view; all state lives in the daemon services.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr,
                        QString daemonConfigPath = QString(),
                        bool autoSpawnDaemon = true);
    ~MainWindow() override;

    /// Opens a repository through the daemon (also used by tests).
    void openRepository(const QString &root);
    void reconnectToDaemon();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onStateChanged(DaemonChannel::State state, const QString &detail);
    void onConnected(const DaemonStatus &status);
    void onDisconnected(const QString &reason);
    void onError(const QString &message);
    void onReposChanged(const QList<RepoInfo> &repos);
    void onActiveRepoChanged(const RepoInfo &repo);
    void onStatusChanged(const QString &root, const StatusSnapshot &snapshot);
    void onTreeChanged(const QString &root, const TreeNodeData &tree);
    void onStatusSelectionChanged();
    void promptForRepository();
    void refreshStatus();
    void stageSelected();
    void unstageSelected();
    void submit();
    void updateWorkspace();
    void showStatusContextMenu(const QPoint &pos);
    void showTreeContextMenu(const QPoint &pos);

    void onCommitsChanged(const QList<CommitInfo> &commits, bool canLoadMore);
    void onCommitDetailChanged(const CommitInfo &commit, const TreeNodeData &tree);
    void onCommitSelectionChanged();
    void onCommitTreeDoubleClicked(const QModelIndex &index);
    void onLocksChanged(const QList<FileLockInfo> &locks);
    void onLockSelectionChanged();
    void onBranchesChanged(const QList<BranchInfo> &branches);
    void onGraphChanged(const QList<CommitInfo> &commits);
    void onBranchSelectionChanged();
    void promptCreateBranch();
    void switchSelectedBranch();
    void deleteSelectedBranch();
    void promptMerge();
    void diffSelectedPending();
    void diffCommit();
    void refreshDiff();
    void exportDiff();
    void unlockSelectedLock();
    void showLocksContextMenu(const QPoint &pos);

private:
    void buildUi();
    void buildActions();
    QWidget *buildPendingPanel();
    QWidget *buildHistoryPage();
    QWidget *buildBranchesPage();
    QWidget *buildGraphPage();
    QWidget *buildDiffPage();
    QWidget *buildLocksPage();
    void appendLog(const QString &message);
    void updateBranchLabel(const StatusSnapshot &snapshot);
    void updateCategoryFilter();
    void selectActiveRepositoryItem();
    bool runOperation(DaemonOperation *operation,
                      const QString &title,
                      OperationResult *result = nullptr);
    void refreshWorkspace();
    void handleMergeConflicts(const QString &sourceBranch, const QStringList &conflicts);
    BranchInfo selectedBranch() const;
    QStringList selectedStatusPaths() const;
    bool selectedStatusStaged() const;
    bool confirmLockedPaths(const QStringList &paths, const QString &action);
    void runDiff(const DiffRequestData &request, const QString &title);
    void applyDiffOptionsToRequest();

    DaemonChannel *channel_ = nullptr;
    RepositoryService *repository_ = nullptr;
    StatusService *status_ = nullptr;
    HistoryService *history_ = nullptr;
    LockService *locks_ = nullptr;
    BranchService *branches_ = nullptr;
    GraphService *graph_ = nullptr;

    StatusModel *statusModel_ = nullptr;
    RepositoryTreeModel *treeModel_ = nullptr;
    CommitLogModel *commitLogModel_ = nullptr;
    RepositoryTreeModel *commitTreeModel_ = nullptr;
    LocksModel *locksModel_ = nullptr;
    BranchModel *branchModel_ = nullptr;
    RevisionGraphModel *graphModel_ = nullptr;

    QTabWidget *centralTabs_ = nullptr;
    QWidget *historyPage_ = nullptr;
    QWidget *branchesPage_ = nullptr;
    QWidget *graphPage_ = nullptr;
    QWidget *diffPage_ = nullptr;
    QWidget *locksPage_ = nullptr;

    QListWidget *repositoriesList_ = nullptr;
    QTreeView *treeView_ = nullptr;
    QTableView *statusTable_ = nullptr;
    QComboBox *categoryFilter_ = nullptr;
    QLabel *repositoryLabel_ = nullptr;
    QLabel *branchLabel_ = nullptr;
    QLabel *connectionLabel_ = nullptr;
    QPlainTextEdit *logEdit_ = nullptr;

    QTableView *commitLogTable_ = nullptr;
    QPushButton *loadMoreCommitsButton_ = nullptr;
    QLabel *commitDetailLabel_ = nullptr;
    QTreeView *commitTreeView_ = nullptr;
    QPushButton *diffCommitButton_ = nullptr;

    DiffView *diffView_ = nullptr;
    QLabel *diffTitleLabel_ = nullptr;
    QComboBox *formatCombo_ = nullptr;
    QCheckBox *stagedCheck_ = nullptr;
    QCheckBox *mergeBaseCheck_ = nullptr;
    QComboBox *whitespaceCombo_ = nullptr;
    QSpinBox *contextSpin_ = nullptr;
    QPushButton *refreshDiffButton_ = nullptr;
    QPushButton *exportDiffButton_ = nullptr;

    QTableView *locksTable_ = nullptr;
    QPushButton *refreshLocksButton_ = nullptr;
    QPushButton *unlockButton_ = nullptr;

    QTableView *branchesTable_ = nullptr;
    QPushButton *newBranchButton_ = nullptr;
    QPushButton *switchBranchButton_ = nullptr;
    QPushButton *deleteBranchButton_ = nullptr;
    QPushButton *refreshBranchesButton_ = nullptr;

    QTableView *graphTable_ = nullptr;
    QPushButton *refreshGraphButton_ = nullptr;

    QAction *openRepositoryAction_ = nullptr;
    QAction *closeRepositoryAction_ = nullptr;
    QAction *refreshAction_ = nullptr;
    QAction *reconnectAction_ = nullptr;
    QAction *quitAction_ = nullptr;
    QAction *aboutAction_ = nullptr;
    QAction *stageAction_ = nullptr;
    QAction *unstageAction_ = nullptr;
    QAction *submitAction_ = nullptr;
    QAction *updateAction_ = nullptr;
    QAction *diffAction_ = nullptr;
    QAction *mergeAction_ = nullptr;

    DaemonDiff *currentDiff_ = nullptr;
    DiffRequestData lastDiffRequest_;
    QString lastDiffRoot_;
    QString lastDiffTitle_;
};
