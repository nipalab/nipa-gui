#include "mainwindow.h"

#include "models/statusmodel.h"
#include "services/repositoryservice.h"
#include "services/statusservice.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QTableView>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent, QString daemonConfigPath, bool autoSpawnDaemon)
    : QMainWindow(parent)
{
    statusModel_ = new StatusModel(this);

    buildUi();
    buildActions();

    channel_ = new DaemonChannel(std::move(daemonConfigPath), this);
    channel_->setAutoSpawnEnabled(autoSpawnDaemon);
    repository_ = new RepositoryService(channel_, this);
    status_ = new StatusService(channel_, repository_, this);

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
    connect(repository_, &RepositoryService::errorOccurred, this, [this](const QString &message) {
        appendLog(tr("Repository error: %1").arg(message));
    });

    connect(status_, &StatusService::statusChanged, this, &MainWindow::onStatusChanged);
    connect(status_, &StatusService::refreshFailed, this, [this](const QString &, const QString &message) {
        appendLog(tr("Status refresh failed: %1").arg(message));
    });

    channel_->start();
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi()
{
    setObjectName(QStringLiteral("mainWindow"));
    setWindowTitle(QStringLiteral("nipa-gui"));
    resize(1100, 700);

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(6, 6, 6, 6);

    auto *header = new QHBoxLayout();
    repositoryLabel_ = new QLabel(tr("No repository open"), central);
    repositoryLabel_->setObjectName(QStringLiteral("repositoryLabel"));
    QFont repositoryFont = repositoryLabel_->font();
    repositoryFont.setBold(true);
    repositoryLabel_->setFont(repositoryFont);

    branchLabel_ = new QLabel(central);
    branchLabel_->setObjectName(QStringLiteral("branchLabel"));

    categoryFilter_ = new QComboBox(central);
    categoryFilter_->setObjectName(QStringLiteral("categoryFilter"));
    connect(categoryFilter_, &QComboBox::currentIndexChanged, this, [this](int index) {
        statusModel_->setCategoryFilter(categoryFilter_->itemData(index).toInt());
    });

    auto *refreshButton = new QPushButton(tr("Refresh"), central);
    refreshButton->setObjectName(QStringLiteral("refreshStatusButton"));
    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::refreshStatus);

    header->addWidget(repositoryLabel_);
    header->addWidget(branchLabel_);
    header->addStretch();
    header->addWidget(new QLabel(tr("Show:"), central));
    header->addWidget(categoryFilter_);
    header->addWidget(refreshButton);

    statusTable_ = new QTableView(central);
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

    layout->addLayout(header);
    layout->addWidget(statusTable_);
    setCentralWidget(central);

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

    auto *logDock = new QDockWidget(tr("Log"), this);
    logDock->setObjectName(QStringLiteral("logDock"));
    logEdit_ = new QPlainTextEdit(logDock);
    logEdit_->setObjectName(QStringLiteral("logEdit"));
    logEdit_->setReadOnly(true);
    logDock->setWidget(logEdit_);
    addDockWidget(Qt::BottomDockWidgetArea, logDock);

    resizeDocks({repositoriesDock}, {300}, Qt::Horizontal);
    resizeDocks({logDock}, {140}, Qt::Vertical);

    connectionLabel_ = new QLabel(tr("Not connected"), this);
    connectionLabel_->setObjectName(QStringLiteral("connectionLabel"));
    statusBar()->addPermanentWidget(connectionLabel_);

    connect(openButton, &QPushButton::clicked, this, &MainWindow::promptForRepository);
    connect(refreshReposButton, &QPushButton::clicked, this, [this] { repository_->refresh(); });
    connect(closeButton, &QPushButton::clicked, this, [this] { repository_->close(); });
}

void MainWindow::buildActions()
{
    openRepositoryAction_ = new QAction(tr("&Open Repository…"), this);
    openRepositoryAction_->setShortcut(QKeySequence::Open);
    connect(openRepositoryAction_, &QAction::triggered, this, &MainWindow::promptForRepository);

    closeRepositoryAction_ = new QAction(tr("&Close Repository"), this);
    connect(closeRepositoryAction_, &QAction::triggered, this, [this] { repository_->close(); });

    refreshAction_ = new QAction(tr("&Refresh Status"), this);
    refreshAction_->setShortcut(QKeySequence::Refresh);
    connect(refreshAction_, &QAction::triggered, this, &MainWindow::refreshStatus);

    reconnectAction_ = new QAction(tr("&Reconnect to Daemon"), this);
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

    auto *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(openRepositoryAction_);
    fileMenu->addAction(closeRepositoryAction_);
    fileMenu->addSeparator();
    fileMenu->addAction(refreshAction_);
    fileMenu->addSeparator();
    fileMenu->addAction(quitAction_);

    auto *daemonMenu = menuBar()->addMenu(tr("&Daemon"));
    daemonMenu->addAction(reconnectAction_);

    auto *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(aboutAction_);

    closeRepositoryAction_->setEnabled(false);
    refreshAction_->setEnabled(false);
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
        selectActiveRepositoryItem();
        return;
    }

    repositoryLabel_->setText(repo.root);
    branchLabel_->setText(repo.branch.isEmpty() ? QString() : tr("Branch: %1").arg(repo.branch));
    setWindowTitle(tr("nipa-gui — %1").arg(repo.root));
    closeRepositoryAction_->setEnabled(true);
    refreshAction_->setEnabled(true);
    selectActiveRepositoryItem();
    appendLog(tr("Opened repository %1").arg(repo.root));
}

void MainWindow::onStatusChanged(const QString &root, const StatusSnapshot &snapshot)
{
    if (root != repository_->activeRoot()) {
        return;
    }
    statusModel_->setSnapshot(snapshot);
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

void MainWindow::closeEvent(QCloseEvent *event)
{
    status_->setAutoRefreshEnabled(false);
    channel_->shutdownIfOwned();
    QMainWindow::closeEvent(event);
}
