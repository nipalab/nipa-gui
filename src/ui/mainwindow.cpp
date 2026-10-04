#include "mainwindow.h"

#include "daemon/daemonoperation.h"
#include "diffview.h"
#include "models/commitlogmodel.h"
#include "models/locksmodel.h"
#include "models/repositorytreemodel.h"
#include "models/statusmodel.h"
#include "operationprogressdialog.h"
#include "services/historyservice.h"
#include "services/lockservice.h"
#include "services/repositoryservice.h"
#include "services/statusservice.h"
#include "submitdialog.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDebug>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableView>
#include <QToolBar>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent, QString daemonConfigPath, bool autoSpawnDaemon)
    : QMainWindow(parent)
{
    statusModel_ = new StatusModel(this);
    treeModel_ = new RepositoryTreeModel(this);
    commitLogModel_ = new CommitLogModel(this);
    commitTreeModel_ = new RepositoryTreeModel(this);
    locksModel_ = new LocksModel(this);

    buildUi();
    buildActions();

    channel_ = new DaemonChannel(std::move(daemonConfigPath), this);
    channel_->setAutoSpawnEnabled(autoSpawnDaemon);
    repository_ = new RepositoryService(channel_, this);
    status_ = new StatusService(channel_, repository_, this);
    history_ = new HistoryService(channel_, repository_, this);
    locks_ = new LockService(channel_, repository_, this);

    connect(channel_, &DaemonChannel::stateChanged, this, &MainWindow::onStateChanged);
    connect(channel_, &DaemonChannel::connected, this, &MainWindow::onConnected);
    connect(channel_, &DaemonChannel::disconnected, this, &MainWindow::onDisconnected);
    connect(channel_, &DaemonChannel::errorOccurred, this, &MainWindow::onError);
    connect(channel_, &DaemonChannel::versionWarning, this,
            [this](const QString &daemonVersion, const QString &minimumVersion) {
                appendLog(tr("Warning: daemon %1 is older than the required %2")
                              .arg(daemonVersion, minimumVersion));
            });

    connect(repository_, &RepositoryService::reposChanged, this, &MainWindow::onReposChanged);
    connect(repository_, &RepositoryService::activeRepoChanged, this, &MainWindow::onActiveRepoChanged);
    connect(repository_, &RepositoryService::treeChanged, this, &MainWindow::onTreeChanged);
    connect(repository_, &RepositoryService::errorOccurred, this, [this](const QString &message) {
        appendLog(tr("Repository error: %1").arg(message));
    });

    connect(status_, &StatusService::statusChanged, this, &MainWindow::onStatusChanged);
    connect(status_, &StatusService::refreshFailed, this, [this](const QString &, const QString &message) {
        appendLog(tr("Status refresh failed: %1").arg(message));
    });
    connect(status_, &StatusService::stageFailed, this, [this](const QString &message) {
        appendLog(tr("Staging failed: %1").arg(message));
        statusBar()->showMessage(tr("Staging failed: %1").arg(message), 5000);
    });

    connect(history_, &HistoryService::commitsChanged, this, &MainWindow::onCommitsChanged);
    connect(history_, &HistoryService::commitDetailChanged, this, &MainWindow::onCommitDetailChanged);
    connect(history_, &HistoryService::errorOccurred, this, [this](const QString &message) {
        appendLog(tr("History error: %1").arg(message));
    });

    connect(locks_, &LockService::locksChanged, this, &MainWindow::onLocksChanged);
    connect(locks_, &LockService::errorOccurred, this, [this](const QString &message) {
        appendLog(tr("Lock error: %1").arg(message));
        statusBar()->showMessage(tr("Lock error: %1").arg(message), 5000);
    });

    channel_->start();
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi()
{
    setObjectName(QStringLiteral("mainWindow"));
    setWindowTitle(QStringLiteral("nipa-gui"));
    resize(1200, 800);

    centralTabs_ = new QTabWidget(this);
    centralTabs_->setObjectName(QStringLiteral("centralTabs"));
    historyPage_ = buildHistoryPage();
    diffPage_ = buildDiffPage();
    locksPage_ = buildLocksPage();
    centralTabs_->addTab(historyPage_, tr("History"));
    centralTabs_->addTab(diffPage_, tr("Diff"));
    centralTabs_->addTab(locksPage_, tr("Locks"));
    setCentralWidget(centralTabs_);

    auto *pendingDock = new QDockWidget(tr("Pending Changes"), this);
    pendingDock->setObjectName(QStringLiteral("pendingDock"));
    pendingDock->setWidget(buildPendingPanel());
    addDockWidget(Qt::BottomDockWidgetArea, pendingDock);

    auto *logDock = new QDockWidget(tr("Log"), this);
    logDock->setObjectName(QStringLiteral("logDock"));
    logEdit_ = new QPlainTextEdit(logDock);
    logEdit_->setObjectName(QStringLiteral("logEdit"));
    logEdit_->setReadOnly(true);
    logDock->setWidget(logEdit_);
    addDockWidget(Qt::BottomDockWidgetArea, logDock);
    tabifyDockWidget(pendingDock, logDock);
    pendingDock->raise();

    auto *repositoriesDock = new QDockWidget(tr("Repositories"), this);
    repositoriesDock->setObjectName(QStringLiteral("repositoriesDock"));
    auto *repositoriesPanel = new QWidget(repositoriesDock);
    auto *repositoriesLayout = new QVBoxLayout(repositoriesPanel);
    repositoriesLayout->setContentsMargins(0, 0, 0, 0);

    repositoriesList_ = new QListWidget(repositoriesPanel);
    repositoriesList_->setObjectName(QStringLiteral("repositoriesList"));
    connect(repositoriesList_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        openRepository(item->data(Qt::UserRole).toString());
    });

    auto *repositoryButtons = new QHBoxLayout();
    auto *openButton = new QPushButton(tr("Open…"), repositoriesPanel);
    openButton->setObjectName(QStringLiteral("openRepositoryButton"));
    auto *refreshReposButton = new QPushButton(tr("Refresh"), repositoriesPanel);
    refreshReposButton->setObjectName(QStringLiteral("refreshReposButton"));
    auto *closeButton = new QPushButton(tr("Close"), repositoriesPanel);
    closeButton->setObjectName(QStringLiteral("closeRepositoryButton"));
    repositoryButtons->addWidget(openButton);
    repositoryButtons->addWidget(refreshReposButton);
    repositoryButtons->addWidget(closeButton);

    repositoriesLayout->addWidget(repositoriesList_);
    repositoriesLayout->addLayout(repositoryButtons);
    repositoriesDock->setWidget(repositoriesPanel);
    addDockWidget(Qt::LeftDockWidgetArea, repositoriesDock);

    auto *treeDock = new QDockWidget(tr("Tree"), this);
    treeDock->setObjectName(QStringLiteral("treeDock"));
    treeView_ = new QTreeView(treeDock);
    treeView_->setObjectName(QStringLiteral("treeView"));
    treeView_->setModel(treeModel_);
    treeView_->setUniformRowHeights(true);
    treeView_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(treeView_, &QTreeView::customContextMenuRequested, this, &MainWindow::showTreeContextMenu);
    treeDock->setWidget(treeView_);
    addDockWidget(Qt::LeftDockWidgetArea, treeDock);
    splitDockWidget(repositoriesDock, treeDock, Qt::Vertical);

    resizeDocks({repositoriesDock}, {300}, Qt::Horizontal);
    resizeDocks({pendingDock}, {260}, Qt::Vertical);

    connectionLabel_ = new QLabel(tr("Not connected"), this);
    connectionLabel_->setObjectName(QStringLiteral("connectionLabel"));
    statusBar()->addPermanentWidget(connectionLabel_);

    connect(openButton, &QPushButton::clicked, this, &MainWindow::promptForRepository);
    connect(refreshReposButton, &QPushButton::clicked, this, [this] { repository_->refresh(); });
    connect(closeButton, &QPushButton::clicked, this, [this] { repository_->close(); });
}

QWidget *MainWindow::buildPendingPanel()
{
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(6, 6, 6, 6);

    auto *header = new QHBoxLayout();
    repositoryLabel_ = new QLabel(tr("No repository open"), panel);
    repositoryLabel_->setObjectName(QStringLiteral("repositoryLabel"));
    QFont repositoryFont = repositoryLabel_->font();
    repositoryFont.setBold(true);
    repositoryLabel_->setFont(repositoryFont);

    branchLabel_ = new QLabel(panel);
    branchLabel_->setObjectName(QStringLiteral("branchLabel"));

    categoryFilter_ = new QComboBox(panel);
    categoryFilter_->setObjectName(QStringLiteral("categoryFilter"));
    connect(categoryFilter_, &QComboBox::currentIndexChanged, this, [this](int index) {
        statusModel_->setCategoryFilter(categoryFilter_->itemData(index).toInt());
    });

    auto *refreshButton = new QPushButton(tr("Refresh"), panel);
    refreshButton->setObjectName(QStringLiteral("refreshStatusButton"));
    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::refreshStatus);

    header->addWidget(repositoryLabel_);
    header->addWidget(branchLabel_);
    header->addStretch();
    header->addWidget(new QLabel(tr("Show:"), panel));
    header->addWidget(categoryFilter_);
    header->addWidget(refreshButton);

    statusTable_ = new QTableView(panel);
    statusTable_->setObjectName(QStringLiteral("statusTable"));
    statusTable_->setModel(statusModel_);
    statusTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    statusTable_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    statusTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    statusTable_->setShowGrid(false);
    statusTable_->setAlternatingRowColors(true);
    statusTable_->verticalHeader()->setVisible(false);
    statusTable_->horizontalHeader()->setStretchLastSection(true);
    statusTable_->horizontalHeader()->setSectionResizeMode(StatusModel::StatusColumn,
                                                          QHeaderView::ResizeToContents);
    statusTable_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(statusTable_, &QTableView::customContextMenuRequested, this,
            &MainWindow::showStatusContextMenu);
    connect(statusTable_->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &MainWindow::onStatusSelectionChanged);

    layout->addLayout(header);
    layout->addWidget(statusTable_);
    return panel;
}

QWidget *MainWindow::buildHistoryPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QHBoxLayout(page);
    layout->setContentsMargins(6, 6, 6, 6);

    auto *splitter = new QSplitter(Qt::Horizontal, page);

    auto *left = new QWidget(splitter);
    auto *leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    commitLogTable_ = new QTableView(left);
    commitLogTable_->setObjectName(QStringLiteral("commitLogTable"));
    commitLogTable_->setModel(commitLogModel_);
    commitLogTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    commitLogTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    commitLogTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    commitLogTable_->setShowGrid(false);
    commitLogTable_->setAlternatingRowColors(true);
    commitLogTable_->verticalHeader()->setVisible(false);
    commitLogTable_->horizontalHeader()->setStretchLastSection(true);
    commitLogTable_->horizontalHeader()->setSectionResizeMode(CommitLogModel::CommitColumn,
                                                             QHeaderView::ResizeToContents);
    loadMoreCommitsButton_ = new QPushButton(tr("Load more"), left);
    loadMoreCommitsButton_->setObjectName(QStringLiteral("loadMoreCommitsButton"));
    loadMoreCommitsButton_->setEnabled(false);
    leftLayout->addWidget(commitLogTable_);
    leftLayout->addWidget(loadMoreCommitsButton_);

    auto *right = new QWidget(splitter);
    auto *rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    commitDetailLabel_ = new QLabel(tr("Select a commit to see its details"), right);
    commitDetailLabel_->setObjectName(QStringLiteral("commitDetailLabel"));
    commitDetailLabel_->setWordWrap(true);
    commitDetailLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    commitTreeView_ = new QTreeView(right);
    commitTreeView_->setObjectName(QStringLiteral("commitTreeView"));
    commitTreeView_->setModel(commitTreeModel_);
    commitTreeView_->setUniformRowHeights(true);
    diffCommitButton_ = new QPushButton(tr("Diff Commit"), right);
    diffCommitButton_->setObjectName(QStringLiteral("diffCommitButton"));
    diffCommitButton_->setEnabled(false);
    rightLayout->addWidget(commitDetailLabel_);
    rightLayout->addWidget(commitTreeView_, 1);
    rightLayout->addWidget(diffCommitButton_);

    splitter->addWidget(left);
    splitter->addWidget(right);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 3);
    layout->addWidget(splitter);

    connect(commitLogTable_->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &MainWindow::onCommitSelectionChanged);
    connect(commitLogTable_, &QTableView::doubleClicked, this, &MainWindow::diffCommit);
    connect(loadMoreCommitsButton_, &QPushButton::clicked, this, [this] { history_->loadMore(); });
    connect(diffCommitButton_, &QPushButton::clicked, this, &MainWindow::diffCommit);
    connect(commitTreeView_, &QTreeView::doubleClicked, this, &MainWindow::onCommitTreeDoubleClicked);
    return page;
}

QWidget *MainWindow::buildDiffPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(6, 6, 6, 6);

    auto *toolbar = new QHBoxLayout();
    formatCombo_ = new QComboBox(page);
    formatCombo_->setObjectName(QStringLiteral("diffFormatCombo"));
    formatCombo_->addItem(tr("Patch"), QStringLiteral("patch"));
    formatCombo_->addItem(tr("Stat"), QStringLiteral("stat"));
    formatCombo_->addItem(tr("Name only"), QStringLiteral("name_only"));
    formatCombo_->addItem(tr("Name status"), QStringLiteral("name_status"));

    stagedCheck_ = new QCheckBox(tr("Staged"), page);
    stagedCheck_->setObjectName(QStringLiteral("diffStagedCheck"));
    stagedCheck_->setToolTip(tr("Compare the staged set instead of the working copy"));

    mergeBaseCheck_ = new QCheckBox(tr("Merge base"), page);
    mergeBaseCheck_->setObjectName(QStringLiteral("diffMergeBaseCheck"));
    mergeBaseCheck_->setToolTip(tr("Three-dot diff against the merge base (needs two revisions)"));
    mergeBaseCheck_->setEnabled(false);

    whitespaceCombo_ = new QComboBox(page);
    whitespaceCombo_->setObjectName(QStringLiteral("diffWhitespaceCombo"));
    whitespaceCombo_->addItem(tr("Whitespace: exact"), QStringLiteral("none"));
    whitespaceCombo_->addItem(tr("Ignore all space"), QStringLiteral("all"));
    whitespaceCombo_->addItem(tr("Ignore space change"), QStringLiteral("change"));

    contextSpin_ = new QSpinBox(page);
    contextSpin_->setObjectName(QStringLiteral("diffContextSpin"));
    contextSpin_->setRange(0, 20);
    contextSpin_->setSpecialValueText(tr("default"));
    contextSpin_->setToolTip(tr("Context lines (0 = default)"));

    refreshDiffButton_ = new QPushButton(tr("Refresh"), page);
    refreshDiffButton_->setObjectName(QStringLiteral("refreshDiffButton"));
    exportDiffButton_ = new QPushButton(tr("Export…"), page);
    exportDiffButton_->setObjectName(QStringLiteral("exportDiffButton"));

    diffTitleLabel_ = new QLabel(page);
    diffTitleLabel_->setObjectName(QStringLiteral("diffTitleLabel"));

    toolbar->addWidget(new QLabel(tr("Format:"), page));
    toolbar->addWidget(formatCombo_);
    toolbar->addWidget(stagedCheck_);
    toolbar->addWidget(mergeBaseCheck_);
    toolbar->addWidget(whitespaceCombo_);
    toolbar->addWidget(new QLabel(tr("Context:"), page));
    toolbar->addWidget(contextSpin_);
    toolbar->addWidget(refreshDiffButton_);
    toolbar->addWidget(exportDiffButton_);
    toolbar->addStretch();
    toolbar->addWidget(diffTitleLabel_);

    diffView_ = new DiffView(page);
    diffView_->setObjectName(QStringLiteral("diffView"));

    layout->addLayout(toolbar);
    layout->addWidget(diffView_, 1);

    connect(refreshDiffButton_, &QPushButton::clicked, this, &MainWindow::refreshDiff);
    connect(exportDiffButton_, &QPushButton::clicked, this, &MainWindow::exportDiff);
    return page;
}

QWidget *MainWindow::buildLocksPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(6, 6, 6, 6);

    auto *toolbar = new QHBoxLayout();
    refreshLocksButton_ = new QPushButton(tr("Refresh"), page);
    refreshLocksButton_->setObjectName(QStringLiteral("refreshLocksButton"));
    unlockButton_ = new QPushButton(tr("Unlock"), page);
    unlockButton_->setObjectName(QStringLiteral("unlockButton"));
    unlockButton_->setEnabled(false);
    toolbar->addWidget(refreshLocksButton_);
    toolbar->addWidget(unlockButton_);
    toolbar->addStretch();

    locksTable_ = new QTableView(page);
    locksTable_->setObjectName(QStringLiteral("locksTable"));
    locksTable_->setModel(locksModel_);
    locksTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    locksTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    locksTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    locksTable_->setShowGrid(false);
    locksTable_->setAlternatingRowColors(true);
    locksTable_->verticalHeader()->setVisible(false);
    locksTable_->horizontalHeader()->setStretchLastSection(true);
    locksTable_->setContextMenuPolicy(Qt::CustomContextMenu);

    layout->addLayout(toolbar);
    layout->addWidget(locksTable_, 1);

    connect(refreshLocksButton_, &QPushButton::clicked, this, [this] { locks_->refresh(); });
    connect(unlockButton_, &QPushButton::clicked, this, &MainWindow::unlockSelectedLock);
    connect(locksTable_->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &MainWindow::onLockSelectionChanged);
    connect(locksTable_, &QTableView::customContextMenuRequested, this,
            &MainWindow::showLocksContextMenu);
    return page;
}

void MainWindow::buildActions()
{
    openRepositoryAction_ = new QAction(tr("&Open Repository…"), this);
    openRepositoryAction_->setObjectName(QStringLiteral("openRepositoryAction"));
    openRepositoryAction_->setShortcut(QKeySequence::Open);
    connect(openRepositoryAction_, &QAction::triggered, this, &MainWindow::promptForRepository);

    closeRepositoryAction_ = new QAction(tr("&Close Repository"), this);
    closeRepositoryAction_->setObjectName(QStringLiteral("closeRepositoryAction"));
    connect(closeRepositoryAction_, &QAction::triggered, this, [this] { repository_->close(); });

    refreshAction_ = new QAction(tr("&Refresh Status"), this);
    refreshAction_->setObjectName(QStringLiteral("refreshAction"));
    refreshAction_->setShortcut(QKeySequence::Refresh);
    connect(refreshAction_, &QAction::triggered, this, &MainWindow::refreshStatus);

    reconnectAction_ = new QAction(tr("&Reconnect to Daemon"), this);
    reconnectAction_->setObjectName(QStringLiteral("reconnectAction"));
    connect(reconnectAction_, &QAction::triggered, this, &MainWindow::reconnectToDaemon);

    quitAction_ = new QAction(tr("&Quit"), this);
    quitAction_->setShortcut(QKeySequence::Quit);
    connect(quitAction_, &QAction::triggered, this, &MainWindow::close);

    aboutAction_ = new QAction(tr("&About nipa-gui"), this);
    connect(aboutAction_, &QAction::triggered, this, [this] {
        QMessageBox::about(this, tr("About nipa-gui"),
                           tr("<b>nipa-gui</b> %1<br>P4V-style desktop client for nipa.<br>"
                              "All operations are served by the local <code>nipa serve</code> daemon.")
                               .arg(QApplication::applicationVersion()));
    });

    updateAction_ = new QAction(tr("&Update"), this);
    updateAction_->setObjectName(QStringLiteral("updateAction"));
    connect(updateAction_, &QAction::triggered, this, &MainWindow::updateWorkspace);

    submitAction_ = new QAction(tr("&Submit…"), this);
    submitAction_->setObjectName(QStringLiteral("submitAction"));
    connect(submitAction_, &QAction::triggered, this, &MainWindow::submit);

    stageAction_ = new QAction(tr("&Stage"), this);
    stageAction_->setObjectName(QStringLiteral("stageAction"));
    connect(stageAction_, &QAction::triggered, this, &MainWindow::stageSelected);

    unstageAction_ = new QAction(tr("&Unstage"), this);
    unstageAction_->setObjectName(QStringLiteral("unstageAction"));
    connect(unstageAction_, &QAction::triggered, this, &MainWindow::unstageSelected);

    diffAction_ = new QAction(tr("&Diff"), this);
    diffAction_->setObjectName(QStringLiteral("diffAction"));
    connect(diffAction_, &QAction::triggered, this, &MainWindow::diffSelectedPending);

    auto *toolBar = addToolBar(tr("Main"));
    toolBar->setObjectName(QStringLiteral("mainToolBar"));
    toolBar->addAction(updateAction_);
    toolBar->addAction(submitAction_);
    toolBar->addSeparator();
    toolBar->addAction(stageAction_);
    toolBar->addAction(unstageAction_);
    toolBar->addAction(diffAction_);
    toolBar->addSeparator();
    toolBar->addAction(refreshAction_);

    auto *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(openRepositoryAction_);
    fileMenu->addAction(closeRepositoryAction_);
    fileMenu->addSeparator();
    fileMenu->addAction(refreshAction_);
    fileMenu->addSeparator();
    fileMenu->addAction(quitAction_);

    auto *daemonMenu = menuBar()->addMenu(tr("&Daemon"));
    daemonMenu->addAction(reconnectAction_);

    auto *actionsMenu = menuBar()->addMenu(tr("&Actions"));
    actionsMenu->addAction(updateAction_);
    actionsMenu->addAction(submitAction_);
    actionsMenu->addSeparator();
    actionsMenu->addAction(stageAction_);
    actionsMenu->addAction(unstageAction_);
    actionsMenu->addAction(diffAction_);

    auto *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(aboutAction_);

    closeRepositoryAction_->setEnabled(false);
    refreshAction_->setEnabled(false);
    updateAction_->setEnabled(false);
    submitAction_->setEnabled(false);
    stageAction_->setEnabled(false);
    unstageAction_->setEnabled(false);
    diffAction_->setEnabled(false);
}

void MainWindow::appendLog(const QString &message)
{
    if (message.isEmpty()) {
        return;
    }
    logEdit_->appendPlainText(message);
    qInfo().noquote() << message;
}

void MainWindow::onStateChanged(DaemonChannel::State state, const QString &detail)
{
    appendLog(detail);
    if (state != DaemonChannel::State::Connected) {
        connectionLabel_->setText(detail.isEmpty() ? tr("Not connected") : detail);
    }
}

void MainWindow::onConnected(const DaemonStatus &status)
{
    connectionLabel_->setText(
        tr("Connected: nipa %1 (pid %2)").arg(status.version, QString::number(status.pid)));
    repository_->refresh();
}

void MainWindow::onDisconnected(const QString &reason)
{
    connectionLabel_->setText(tr("Not connected: %1").arg(reason));
    appendLog(tr("Disconnected: %1").arg(reason));
}

void MainWindow::onError(const QString &message)
{
    connectionLabel_->setText(tr("Not connected: %1").arg(message));
    appendLog(tr("Error: %1").arg(message));
}

void MainWindow::onReposChanged(const QList<RepoInfo> &repos)
{
    repositoriesList_->clear();
    for (const RepoInfo &repo : repos) {
        auto *item = new QListWidgetItem(repo.root, repositoriesList_);
        item->setData(Qt::UserRole, repo.root);
        item->setToolTip(tr("%1\nbranch: %2").arg(repo.url, repo.branch));
    }
    selectActiveRepositoryItem();
    appendLog(repos.size() == 1 ? tr("1 repository registered with the daemon")
                                : tr("%1 repositories registered with the daemon").arg(repos.size()));
}

void MainWindow::onActiveRepoChanged(const RepoInfo &repo)
{
    if (repo.root.isEmpty()) {
        repositoryLabel_->setText(tr("No repository open"));
        branchLabel_->clear();
        statusModel_->clear();
        updateCategoryFilter();
        setWindowTitle(QStringLiteral("nipa-gui"));
        closeRepositoryAction_->setEnabled(false);
        refreshAction_->setEnabled(false);
        updateAction_->setEnabled(false);
        submitAction_->setEnabled(false);
        diffAction_->setEnabled(false);
        if (currentDiff_ != nullptr) {
            currentDiff_->cancel();
            currentDiff_ = nullptr;
        }
        diffView_->clearDocument();
        diffTitleLabel_->clear();
        lastDiffRequest_ = {};
        lastDiffRoot_.clear();
        lastDiffTitle_.clear();
        selectActiveRepositoryItem();
        return;
    }

    repositoryLabel_->setText(repo.root);
    branchLabel_->setText(repo.branch.isEmpty() ? QString() : tr("Branch: %1").arg(repo.branch));
    setWindowTitle(tr("nipa-gui — %1").arg(repo.root));
    closeRepositoryAction_->setEnabled(true);
    refreshAction_->setEnabled(true);
    updateAction_->setEnabled(true);
    submitAction_->setEnabled(false);
    selectActiveRepositoryItem();
    appendLog(tr("Opened repository %1").arg(repo.root));
}

void MainWindow::onStatusChanged(const QString &root, const StatusSnapshot &snapshot)
{
    if (root != repository_->activeRoot()) {
        return;
    }
    statusModel_->setSnapshot(snapshot);
    submitAction_->setEnabled(!snapshot.staged.isEmpty());
    updateCategoryFilter();
    updateBranchLabel(snapshot);
}

void MainWindow::updateBranchLabel(const StatusSnapshot &snapshot)
{
    if (repository_->activeRoot().isEmpty()) {
        branchLabel_->clear();
        return;
    }
    if (snapshot.detached()) {
        branchLabel_->setText(tr("Detached at %1 %2").arg(snapshot.headKind, snapshot.headName));
    } else if (!snapshot.branch.isEmpty()) {
        branchLabel_->setText(tr("Branch: %1").arg(snapshot.branch));
    } else if (!repository_->activeRepo().branch.isEmpty()) {
        branchLabel_->setText(tr("Branch: %1").arg(repository_->activeRepo().branch));
    } else {
        branchLabel_->clear();
    }
}

void MainWindow::updateCategoryFilter()
{
    const int current = categoryFilter_->currentData().toInt();
    const QSignalBlocker blocker(categoryFilter_);
    categoryFilter_->clear();
    categoryFilter_->addItem(tr("All (%1)").arg(statusModel_->snapshot().total()), -1);
    for (int category = 0; category < StatusModel::CategoryCount; ++category) {
        const auto typed = static_cast<StatusModel::Category>(category);
        categoryFilter_->addItem(tr("%1 (%2)")
                                     .arg(StatusModel::categoryLabel(typed))
                                     .arg(statusModel_->count(typed)),
                                 category);
    }
    const int index = categoryFilter_->findData(current);
    categoryFilter_->setCurrentIndex(index >= 0 ? index : 0);
}

void MainWindow::selectActiveRepositoryItem()
{
    const QString activeRoot = repository_->activeRoot();
    for (int row = 0; row < repositoriesList_->count(); ++row) {
        QListWidgetItem *item = repositoriesList_->item(row);
        if (item->data(Qt::UserRole).toString() == activeRoot && !activeRoot.isEmpty()) {
            repositoriesList_->setCurrentItem(item);
            return;
        }
    }
    repositoriesList_->setCurrentItem(nullptr);
}

void MainWindow::promptForRepository()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Open Repository"));
    dialog.resize(520, 320);

    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(tr("Repositories registered with the daemon:"), &dialog));

    auto *list = new QListWidget(&dialog);
    for (const RepoInfo &repo : repository_->repos()) {
        const QString label = repo.branch.isEmpty() ? repo.root : tr("%1  [%2]").arg(repo.root, repo.branch);
        auto *item = new QListWidgetItem(label, list);
        item->setData(Qt::UserRole, repo.root);
    }
    if (list->count() > 0) {
        list->setCurrentRow(0);
    }
    layout->addWidget(list);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    auto *browseButton = buttons->addButton(tr("Browse…"), QDialogButtonBox::ActionRole);
    layout->addWidget(buttons);

    constexpr int kBrowseResult = QDialog::Accepted + 1;
    connect(browseButton, &QPushButton::clicked, &dialog, [&dialog] { dialog.done(kBrowseResult); });
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    const int result = dialog.exec();
    QString selected;
    if (result == QDialog::Accepted && list->currentItem() != nullptr) {
        selected = list->currentItem()->data(Qt::UserRole).toString();
    } else if (result == kBrowseResult) {
        selected = QFileDialog::getExistingDirectory(this, tr("Select a nipa repository"));
    }
    if (!selected.isEmpty()) {
        openRepository(selected);
    }
}

void MainWindow::openRepository(const QString &root)
{
    if (root.isEmpty()) {
        return;
    }
    appendLog(tr("Opening repository %1…").arg(root));
    repository_->open(root);
}

void MainWindow::reconnectToDaemon()
{
    appendLog(tr("Reconnecting to the nipa daemon…"));
    channel_->reconnect();
}

void MainWindow::refreshStatus()
{
    status_->refresh(false);
}

void MainWindow::onTreeChanged(const QString &root, const TreeNodeData &tree)
{
    if (root.isEmpty() || root != repository_->activeRoot()) {
        treeModel_->clear();
        return;
    }
    treeModel_->setTree(tree);
}

void MainWindow::onStatusSelectionChanged()
{
    const bool hasSelection = statusTable_->selectionModel() != nullptr
                              && !statusTable_->selectionModel()->selectedRows().isEmpty();
    stageAction_->setEnabled(hasSelection);
    unstageAction_->setEnabled(hasSelection);
    diffAction_->setEnabled(hasSelection);
}

QStringList MainWindow::selectedStatusPaths() const
{
    QStringList paths;
    if (statusTable_->selectionModel() == nullptr) {
        return paths;
    }
    const QModelIndexList rows = statusTable_->selectionModel()->selectedRows(StatusModel::PathColumn);
    for (const QModelIndex &index : rows) {
        const QString path = index.data(Qt::DisplayRole).toString();
        if (!path.isEmpty() && !paths.contains(path)) {
            paths.append(path);
        }
    }
    return paths;
}

bool MainWindow::selectedStatusStaged() const
{
    if (statusTable_->selectionModel() == nullptr) {
        return false;
    }
    const QModelIndexList rows = statusTable_->selectionModel()->selectedRows();
    if (rows.isEmpty()) {
        return false;
    }
    for (const QModelIndex &index : rows) {
        if (index.data(StatusModel::CategoryRole).toInt() != static_cast<int>(StatusModel::Staged)) {
            return false;
        }
    }
    return true;
}

bool MainWindow::confirmLockedPaths(const QStringList &paths, const QString &action)
{
    QStringList locked;
    for (const QString &path : paths) {
        const QList<FileLockInfo> matches = locks_->locksForPath(path);
        for (const FileLockInfo &lock : matches) {
            const QString holder = lock.heldByName.isEmpty() ? lock.heldBy : lock.heldByName;
            const QString entry = tr("%1 (held by %2)").arg(lock.path, holder);
            if (!locked.contains(entry)) {
                locked.append(entry);
            }
        }
    }
    if (locked.isEmpty()) {
        return true;
    }
    const auto answer = QMessageBox::warning(
        this, tr("Locked files"),
        tr("These paths are locked:\n\n%1\n\nContinue %2?").arg(locked.join(QLatin1Char('\n')), action),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    return answer == QMessageBox::Yes;
}

void MainWindow::stageSelected()
{
    const QStringList paths = selectedStatusPaths();
    if (paths.isEmpty()) {
        return;
    }
    if (!confirmLockedPaths(paths, tr("staging"))) {
        return;
    }
    status_->stage(paths, {});
}

void MainWindow::unstageSelected()
{
    const QStringList paths = selectedStatusPaths();
    if (!paths.isEmpty()) {
        status_->stage({}, paths);
    }
}

void MainWindow::submit()
{
    if (!repository_->hasActiveRepo()) {
        return;
    }
    const StatusSnapshot snapshot = status_->snapshot();
    if (snapshot.staged.isEmpty()) {
        QMessageBox::information(this, tr("Submit"), tr("There is nothing staged to submit."));
        return;
    }
    if (!confirmLockedPaths(snapshot.staged, tr("submitting"))) {
        return;
    }
    SubmitDialog dialog(snapshot.staged, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    runOperation(channel_->push(repository_->activeRoot(), dialog.message()), tr("Submit"));
}

void MainWindow::updateWorkspace()
{
    if (!repository_->hasActiveRepo()) {
        return;
    }
    runOperation(channel_->update(repository_->activeRoot()), tr("Update"));
}

void MainWindow::runOperation(DaemonOperation *operation, const QString &title)
{
    OperationProgressDialog dialog(operation, title, this);
    const bool ok = dialog.run();
    if (ok) {
        appendLog(dialog.resultSummary());
        statusBar()->showMessage(dialog.resultSummary(), 5000);
        status_->refresh(false);
        repository_->refreshTree();
        locks_->refresh();
        return;
    }
    if (dialog.wasCancelled()) {
        appendLog(tr("%1 cancelled").arg(title));
        return;
    }
    appendLog(tr("%1 failed: %2").arg(title, dialog.errorMessage()));
    QMessageBox::warning(this, title, dialog.errorMessage());
}

void MainWindow::showStatusContextMenu(const QPoint &pos)
{
    const bool hasSelection = !selectedStatusPaths().isEmpty();
    QMenu menu(this);
    QAction *stageEntry = menu.addAction(tr("Stage"));
    QAction *unstageEntry = menu.addAction(tr("Unstage"));
    QAction *diffEntry = menu.addAction(tr("Diff"));
    menu.addSeparator();
    QAction *submitEntry = menu.addAction(tr("Submit…"));
    QAction *updateEntry = menu.addAction(tr("Update"));
    stageEntry->setEnabled(hasSelection);
    unstageEntry->setEnabled(hasSelection);
    diffEntry->setEnabled(hasSelection);
    submitEntry->setEnabled(!status_->snapshot().staged.isEmpty());
    updateEntry->setEnabled(repository_->hasActiveRepo());

    QAction *chosen = menu.exec(statusTable_->viewport()->mapToGlobal(pos));
    if (chosen == nullptr) {
        return;
    }
    if (chosen == stageEntry) {
        stageSelected();
    } else if (chosen == unstageEntry) {
        unstageSelected();
    } else if (chosen == diffEntry) {
        diffSelectedPending();
    } else if (chosen == submitEntry) {
        submit();
    } else if (chosen == updateEntry) {
        updateWorkspace();
    }
}

void MainWindow::showTreeContextMenu(const QPoint &pos)
{
    const QModelIndex index = treeView_->indexAt(pos);
    QMenu menu(this);
    QAction *diffEntry = menu.addAction(tr("Diff (working)"));
    QAction *stageEntry = menu.addAction(tr("Stage"));
    QAction *unstageEntry = menu.addAction(tr("Unstage"));
    menu.addSeparator();
    QAction *lockEntry = menu.addAction(tr("Lock"));
    QAction *unlockEntry = menu.addAction(tr("Unlock"));
    menu.addSeparator();
    QAction *revealEntry = menu.addAction(tr("Reveal in File Manager"));
    diffEntry->setEnabled(index.isValid());
    stageEntry->setEnabled(index.isValid());
    unstageEntry->setEnabled(index.isValid());
    lockEntry->setEnabled(index.isValid() && repository_->hasActiveRepo());
    unlockEntry->setEnabled(index.isValid() && repository_->hasActiveRepo());
    revealEntry->setEnabled(index.isValid() && repository_->hasActiveRepo());

    QAction *chosen = menu.exec(treeView_->viewport()->mapToGlobal(pos));
    if (chosen == nullptr || !index.isValid()) {
        return;
    }
    const QString path = treeModel_->pathForIndex(index);
    if (chosen == diffEntry) {
        DiffRequestData request;
        request.paths = {path};
        runDiff(request, tr("Working diff: %1").arg(path));
    } else if (chosen == stageEntry) {
        if (confirmLockedPaths({path}, tr("staging"))) {
            status_->stage({path}, {});
        }
    } else if (chosen == unstageEntry) {
        status_->stage({}, {path});
    } else if (chosen == lockEntry) {
        locks_->lock(path);
    } else if (chosen == unlockEntry) {
        locks_->unlock(path);
    } else if (chosen == revealEntry) {
        const QString absolute = repository_->activeRoot() + QLatin1Char('/') + path;
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(absolute).absolutePath()));
    }
}

void MainWindow::onCommitsChanged(const QList<CommitInfo> &commits, bool canLoadMore)
{
    commitLogModel_->setCommits(commits);
    loadMoreCommitsButton_->setEnabled(canLoadMore);
    const int index = centralTabs_->indexOf(historyPage_);
    if (index >= 0) {
        centralTabs_->setTabText(index, commits.isEmpty() ? tr("History")
                                                          : tr("History (%1)").arg(commits.size()));
    }
    if (commits.isEmpty()) {
        commitDetailLabel_->setText(tr("No commits"));
        commitTreeModel_->clear();
        diffCommitButton_->setEnabled(false);
        return;
    }
    if (commitLogTable_->selectionModel()->selectedRows().isEmpty()) {
        commitLogTable_->selectRow(0);
    }
}

void MainWindow::onCommitDetailChanged(const CommitInfo &commit, const TreeNodeData &tree)
{
    QString parents = commit.parent1;
    if (!commit.parent2.isEmpty()) {
        parents += QStringLiteral(", ") + commit.parent2;
    }
    if (parents.isEmpty()) {
        parents = tr("(root)");
    }
    commitDetailLabel_->setText(
        tr("Commit: %1\nHash: %2\nAuthor: %3 <%4>\nDate: %5\nParents: %6\n\n%7")
            .arg(commit.id, commit.hash, commit.authorName, commit.authorEmail,
                 commit.createdAt.toLocalTime().toString(Qt::ISODate), parents, commit.message));
    commitTreeModel_->setTree(tree);
    diffCommitButton_->setEnabled(!commit.parent1.isEmpty());
}

void MainWindow::onCommitSelectionChanged()
{
    const QModelIndexList rows = commitLogTable_->selectionModel()->selectedRows();
    if (rows.isEmpty()) {
        return;
    }
    history_->selectCommit(commitLogModel_->commitIdAt(rows.first().row()));
}

void MainWindow::onCommitTreeDoubleClicked(const QModelIndex &index)
{
    if (!index.isValid() || commitTreeModel_->isDirectory(index)) {
        return;
    }
    const CommitInfo commit = history_->selectedCommit();
    if (commit.parent1.isEmpty()) {
        appendLog(tr("Commit %1 is a root commit; nothing to diff against").arg(commit.id.left(8)));
        return;
    }
    DiffRequestData request;
    request.revisions = {commit.parent1, commit.id};
    request.paths = {commitTreeModel_->pathForIndex(index)};
    runDiff(request, tr("%1 @ %2").arg(request.paths.first(), commit.id.left(8)));
}

void MainWindow::onLocksChanged(const QList<FileLockInfo> &locks)
{
    locksModel_->setLocks(locks);
    const int index = centralTabs_->indexOf(locksPage_);
    if (index >= 0) {
        centralTabs_->setTabText(index, locks.isEmpty() ? tr("Locks")
                                                        : tr("Locks (%1)").arg(locks.size()));
    }
}

void MainWindow::onLockSelectionChanged()
{
    const bool hasSelection = locksTable_->selectionModel() != nullptr
                              && !locksTable_->selectionModel()->selectedRows().isEmpty();
    unlockButton_->setEnabled(hasSelection);
}

void MainWindow::diffSelectedPending()
{
    const QStringList paths = selectedStatusPaths();
    if (paths.isEmpty()) {
        return;
    }
    DiffRequestData request;
    request.paths = paths;
    request.staged = selectedStatusStaged();
    runDiff(request, request.staged ? tr("Staged diff") : tr("Working diff"));
}

void MainWindow::diffCommit()
{
    if (!repository_->hasActiveRepo()) {
        return;
    }
    const CommitInfo commit = history_->selectedCommit();
    if (commit.id.isEmpty()) {
        return;
    }
    if (commit.parent1.isEmpty()) {
        appendLog(tr("Commit %1 is a root commit; nothing to diff against").arg(commit.id.left(8)));
        return;
    }
    DiffRequestData request;
    request.revisions = {commit.parent1, commit.id};
    runDiff(request, tr("Commit %1").arg(commit.id.left(8)));
}

void MainWindow::refreshDiff()
{
    if (lastDiffRoot_.isEmpty() || lastDiffRoot_ != repository_->activeRoot()) {
        return;
    }
    applyDiffOptionsToRequest();
    runDiff(lastDiffRequest_, lastDiffTitle_);
}

void MainWindow::exportDiff()
{
    const QString text = diffView_->toPlainText();
    if (text.isEmpty()) {
        return;
    }
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export Diff"), QString(),
        tr("Patch files (*.patch *.diff);;Text files (*.txt);;All files (*)"));
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Export Diff"),
                             tr("Cannot write %1: %2").arg(path, file.errorString()));
        return;
    }
    file.write(text.toUtf8());
    appendLog(tr("Diff exported to %1").arg(path));
}

void MainWindow::applyDiffOptionsToRequest()
{
    lastDiffRequest_.format = formatCombo_->currentData().toString();
    if (stagedCheck_->isEnabled()) {
        lastDiffRequest_.staged = stagedCheck_->isChecked();
    }
    const QString whitespace = whitespaceCombo_->currentData().toString();
    lastDiffRequest_.ignoreAllSpace = whitespace == QLatin1String("all");
    lastDiffRequest_.ignoreSpaceChange = whitespace == QLatin1String("change");
    lastDiffRequest_.context = contextSpin_->value();
    if (mergeBaseCheck_->isEnabled()) {
        lastDiffRequest_.mergeBase = mergeBaseCheck_->isChecked();
    }
}

void MainWindow::runDiff(const DiffRequestData &request, const QString &title)
{
    if (!repository_->hasActiveRepo()) {
        return;
    }
    if (currentDiff_ != nullptr) {
        currentDiff_->cancel();
        currentDiff_ = nullptr;
    }

    lastDiffRequest_ = request;
    lastDiffRoot_ = repository_->activeRoot();
    lastDiffTitle_ = title;

    {
        const QSignalBlocker blockFormat(formatCombo_);
        const QSignalBlocker blockStaged(stagedCheck_);
        const QSignalBlocker blockMergeBase(mergeBaseCheck_);
        const QSignalBlocker blockWhitespace(whitespaceCombo_);
        const QSignalBlocker blockContext(contextSpin_);
        formatCombo_->setCurrentIndex(qMax(0, formatCombo_->findData(request.format)));
        stagedCheck_->setChecked(request.staged);
        stagedCheck_->setEnabled(request.revisions.isEmpty());
        mergeBaseCheck_->setChecked(request.mergeBase);
        mergeBaseCheck_->setEnabled(request.revisions.size() == 2);
        QString whitespace = QStringLiteral("none");
        if (request.ignoreAllSpace) {
            whitespace = QStringLiteral("all");
        } else if (request.ignoreSpaceChange) {
            whitespace = QStringLiteral("change");
        }
        whitespaceCombo_->setCurrentIndex(qMax(0, whitespaceCombo_->findData(whitespace)));
        contextSpin_->setValue(request.context);
    }

    diffView_->clearDocument();
    diffTitleLabel_->setText(title);
    centralTabs_->setCurrentWidget(diffPage_);

    currentDiff_ = channel_->diff(lastDiffRoot_, request);
    connect(currentDiff_, &DaemonDiff::dataReceived, this,
            [this, stream = currentDiff_](const QByteArray &chunk) {
                if (stream != currentDiff_) {
                    return;
                }
                diffView_->appendChunk(chunk);
            });
    connect(currentDiff_, &DaemonDiff::finished, this, [this, stream = currentDiff_] {
        if (stream != currentDiff_) {
            return;
        }
        diffView_->finishDocument();
        statusBar()->showMessage(tr("Diff complete"), 3000);
    });
    connect(currentDiff_, &DaemonDiff::failed, this,
            [this, stream = currentDiff_](int code, const QString &message) {
                if (stream != currentDiff_) {
                    return;
                }
                diffView_->appendMessage(tr("Diff failed: %1").arg(message));
                appendLog(tr("Diff failed (%1): %2").arg(code).arg(message));
            });
    connect(currentDiff_, &DaemonDiff::cancelled, this, [this, stream = currentDiff_] {
        if (stream != currentDiff_) {
            return;
        }
        diffView_->appendMessage(tr("(cancelled)"));
    });
}

void MainWindow::unlockSelectedLock()
{
    if (locksTable_->selectionModel() == nullptr) {
        return;
    }
    const QModelIndexList rows = locksTable_->selectionModel()->selectedRows();
    if (rows.isEmpty()) {
        return;
    }
    const QString path = locksModel_->pathAt(rows.first().row());
    if (!path.isEmpty()) {
        locks_->unlock(path);
    }
}

void MainWindow::showLocksContextMenu(const QPoint &pos)
{
    const QModelIndex index = locksTable_->indexAt(pos);
    QMenu menu(this);
    QAction *unlockEntry = menu.addAction(tr("Unlock"));
    unlockEntry->setEnabled(index.isValid());
    if (menu.exec(locksTable_->viewport()->mapToGlobal(pos)) == unlockEntry) {
        unlockSelectedLock();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (currentDiff_ != nullptr) {
        currentDiff_->cancel();
        currentDiff_ = nullptr;
    }
    status_->setAutoRefreshEnabled(false);
    channel_->shutdownIfOwned();
    QMainWindow::closeEvent(event);
}
