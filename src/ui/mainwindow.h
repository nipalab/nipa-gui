#pragma once

#include <QMainWindow>
#include <QString>

#include "daemon/daemonchannel.h"
#include "daemon/daemontypes.h"

class QAction;
class QComboBox;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QTableView;

class StatusModel;
class StatusService;
class WorkspaceService;

/// P4V-style shell: workspaces dock on the left, pending-changes table in the
/// center, daemon log at the bottom. The window is a pure view; all state
/// lives in the daemon services.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr,
                        QString daemonConfigPath = QString(),
                        bool autoSpawnDaemon = true);
    ~MainWindow() override;

    /// Opens a working copy through the daemon (also used by tests).
    void openWorkspace(const QString &root);
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
    void promptForWorkspace();
    void refreshStatus();

private:
    void buildUi();
    void buildActions();
    void appendLog(const QString &message);
    void updateBranchLabel(const StatusSnapshot &snapshot);
    void updateCategoryFilter();
    void selectActiveWorkspaceItem();

    DaemonChannel *channel_ = nullptr;
    WorkspaceService *workspace_ = nullptr;
    StatusService *status_ = nullptr;
    StatusModel *statusModel_ = nullptr;

    QListWidget *workspacesList_ = nullptr;
    QTableView *statusTable_ = nullptr;
    QComboBox *categoryFilter_ = nullptr;
    QLabel *workspaceLabel_ = nullptr;
    QLabel *branchLabel_ = nullptr;
    QLabel *connectionLabel_ = nullptr;
    QPlainTextEdit *logEdit_ = nullptr;

    QAction *openWorkspaceAction_ = nullptr;
    QAction *closeWorkspaceAction_ = nullptr;
    QAction *refreshAction_ = nullptr;
    QAction *reconnectAction_ = nullptr;
    QAction *quitAction_ = nullptr;
    QAction *aboutAction_ = nullptr;
};
