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

class RepositoryService;
class StatusModel;
class StatusService;

/// P4V-style shell: repositories dock on the left, pending-changes table in the
/// center, daemon log at the bottom. The window is a pure view; all state
/// lives in the daemon services.
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
    void promptForRepository();
    void refreshStatus();

private:
    void buildUi();
    void buildActions();
    void appendLog(const QString &message);
    void updateBranchLabel(const StatusSnapshot &snapshot);
    void updateCategoryFilter();
    void selectActiveRepositoryItem();

    DaemonChannel *channel_ = nullptr;
    RepositoryService *repository_ = nullptr;
    StatusService *status_ = nullptr;
    StatusModel *statusModel_ = nullptr;

    QListWidget *repositoriesList_ = nullptr;
    QTableView *statusTable_ = nullptr;
    QComboBox *categoryFilter_ = nullptr;
    QLabel *repositoryLabel_ = nullptr;
    QLabel *branchLabel_ = nullptr;
    QLabel *connectionLabel_ = nullptr;
    QPlainTextEdit *logEdit_ = nullptr;

    QAction *openRepositoryAction_ = nullptr;
    QAction *closeRepositoryAction_ = nullptr;
    QAction *refreshAction_ = nullptr;
    QAction *reconnectAction_ = nullptr;
    QAction *quitAction_ = nullptr;
    QAction *aboutAction_ = nullptr;
};
