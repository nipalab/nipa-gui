#include "ui/mainwindow.h"

#include "fakedaemon.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableView>
#include <QTemporaryDir>
#include <QTreeView>
#include <QtTest>

class MainWindowTest : public QObject {
    Q_OBJECT

private slots:
    void showsDisconnectedStatus();
    void reconnectsAfterDaemonAppears();
    void showsRepositoryStatus();
    void stagesSelectedPendingFiles();
    void showsHistoryAndLocks();
    void diffsSelectedPendingFile();
    void showsBranchesAndGraph();
};

void MainWindowTest::showsDisconnectedStatus()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    MainWindow window(nullptr, tmp.path() + QStringLiteral("/missing/daemon.json"), false);
    auto *label = window.findChild<QLabel *>(QStringLiteral("connectionLabel"));
    QVERIFY(label != nullptr);
    QVERIFY2(label->text().startsWith(QStringLiteral("Not connected:")), qPrintable(label->text()));
}

void MainWindowTest::reconnectsAfterDaemonAppears()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString configPath = tmp.path() + QStringLiteral("/nipa/daemon.json");

    MainWindow window(nullptr, configPath, false);
    auto *label = window.findChild<QLabel *>(QStringLiteral("connectionLabel"));
    QVERIFY(label != nullptr);
    QVERIFY(label->text().startsWith(QStringLiteral("Not connected:")));

    FakeDaemon daemon;
    QVERIFY(daemon.port() > 0);
    QVERIFY(!writeDaemonFile(configPath, daemon.port(), QStringLiteral("token")).isEmpty());

    window.reconnectToDaemon();
    QTRY_VERIFY_WITH_TIMEOUT(label->text().contains(QStringLiteral("9.9.9")), 10000);
    QVERIFY(label->text().startsWith(QStringLiteral("Connected:")));
}

void MainWindowTest::showsRepositoryStatus()
{
    FakeDaemon daemon;
    nipadaemon::RepoInfo repo;
    repo.set_root("/work/assets");
    repo.set_url("https://nipa.example/org/proj");
    repo.set_branch("main");
    daemon.service().repos.push_back(repo);
    daemon.service().watchedRepos["/work/assets"] = repo;

    nipadaemon::StatusResponse status;
    status.set_branch("main");
    status.add_modified("textures/diffuse.png");
    status.add_untracked("notes.txt");
    daemon.service().statuses["/work/assets"] = status;

    greet::GetTreeManifestResponse manifest;
    auto *root = manifest.mutable_root_tree();
    root->set_path("root");
    auto *readme = root->add_files();
    readme->set_path("README.md");
    auto *textures = root->add_sub_trees();
    textures->set_path("textures");
    auto *diffuse = textures->add_files();
    diffuse->set_path("diffuse.png");
    daemon.service().treeManifests["/work/assets"] = manifest;

    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString configPath = tmp.path() + QStringLiteral("/nipa/daemon.json");
    QVERIFY(!writeDaemonFile(configPath, daemon.port(), QStringLiteral("token")).isEmpty());

    MainWindow window(nullptr, configPath, false);
    auto *connection = window.findChild<QLabel *>(QStringLiteral("connectionLabel"));
    QTRY_VERIFY_WITH_TIMEOUT(connection->text().contains(QStringLiteral("9.9.9")), 10000);

    window.openRepository(QStringLiteral("/work/assets"));

    auto *table = window.findChild<QTableView *>(QStringLiteral("statusTable"));
    QVERIFY(table != nullptr);
    QTRY_COMPARE(table->model()->rowCount(), 2);

    auto *branch = window.findChild<QLabel *>(QStringLiteral("branchLabel"));
    QVERIFY(branch != nullptr);
    QTRY_VERIFY(branch->text().contains(QStringLiteral("main")));

    auto *repositoryLabel = window.findChild<QLabel *>(QStringLiteral("repositoryLabel"));
    QVERIFY(repositoryLabel != nullptr);
    QCOMPARE(repositoryLabel->text(), QStringLiteral("/work/assets"));

    auto *tree = window.findChild<QTreeView *>(QStringLiteral("treeView"));
    QVERIFY(tree != nullptr);
    // Directories sort first: textures, then README.md.
    QTRY_COMPARE(tree->model()->rowCount(), 2);
    QCOMPARE(tree->model()->index(0, 0).data().toString(), QStringLiteral("textures"));
    QCOMPARE(tree->model()->index(1, 0).data().toString(), QStringLiteral("README.md"));

    QCOMPARE(daemon.service().watchedRoots.size(), std::size_t(1));
    QCOMPARE(daemon.service().statusRequests.size(), std::size_t(1));
    QCOMPARE(daemon.service().treeRequests, 1);

    auto *closeAction = window.findChild<QAction *>(QStringLiteral("closeRepositoryAction"));
    QVERIFY(closeAction != nullptr);
    QVERIFY(closeAction->isEnabled());
    closeAction->trigger();
    QCOMPARE(tree->model()->rowCount(), 0);
    QCOMPARE(repositoryLabel->text(), QStringLiteral("No repository open"));
    QCOMPARE(table->model()->rowCount(), 0);
}

void MainWindowTest::stagesSelectedPendingFiles()
{
    FakeDaemon daemon;
    nipadaemon::RepoInfo repo;
    repo.set_root("/work/assets");
    repo.set_branch("main");
    daemon.service().repos.push_back(repo);
    daemon.service().watchedRepos["/work/assets"] = repo;

    nipadaemon::StatusResponse status;
    status.set_branch("main");
    status.add_untracked("notes.txt");
    daemon.service().statuses["/work/assets"] = status;

    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString configPath = tmp.path() + QStringLiteral("/nipa/daemon.json");
    QVERIFY(!writeDaemonFile(configPath, daemon.port(), QStringLiteral("token")).isEmpty());

    MainWindow window(nullptr, configPath, false);
    auto *connection = window.findChild<QLabel *>(QStringLiteral("connectionLabel"));
    QTRY_VERIFY_WITH_TIMEOUT(connection->text().contains(QStringLiteral("9.9.9")), 10000);
    window.openRepository(QStringLiteral("/work/assets"));

    auto *table = window.findChild<QTableView *>(QStringLiteral("statusTable"));
    QVERIFY(table != nullptr);
    QTRY_COMPARE(table->model()->rowCount(), 1);

    auto *submitAction = window.findChild<QAction *>(QStringLiteral("submitAction"));
    QVERIFY(submitAction != nullptr);
    QVERIFY(!submitAction->isEnabled());

    table->selectRow(0);
    auto *stageAction = window.findChild<QAction *>(QStringLiteral("stageAction"));
    QVERIFY(stageAction != nullptr);
    QTRY_VERIFY(stageAction->isEnabled());
    stageAction->trigger();

    QTRY_COMPARE(daemon.service().stageRequests.size(), std::size_t(1));
    const nipadaemon::StageRequest &request = daemon.service().stageRequests[0];
    QCOMPARE(request.add_size(), 1);
    QCOMPARE(QString::fromStdString(request.add(0)), QStringLiteral("notes.txt"));

    // Once the daemon reports staged files, Submit becomes available.
    nipadaemon::StatusResponse staged;
    staged.set_branch("main");
    staged.add_staged("notes.txt");
    daemon.service().statuses["/work/assets"] = staged;
    auto *refreshButton = window.findChild<QPushButton *>(QStringLiteral("refreshStatusButton"));
    QVERIFY(refreshButton != nullptr);
    refreshButton->click();
    QTRY_VERIFY(submitAction->isEnabled());
}

void MainWindowTest::showsHistoryAndLocks()
{
    FakeDaemon daemon;
    nipadaemon::RepoInfo repo;
    repo.set_root("/work/assets");
    repo.set_branch("main");
    daemon.service().repos.push_back(repo);
    daemon.service().watchedRepos["/work/assets"] = repo;

    nipadaemon::StatusResponse status;
    status.set_branch("main");
    daemon.service().statuses["/work/assets"] = status;

    greet::GetCommitLogResponse log;
    log.set_branch("main");
    auto *newest = log.add_commits();
    newest->set_commit_id("c1");
    newest->set_commit_hash("h1");
    newest->set_author_name("Ada");
    newest->set_message("newest commit");
    newest->mutable_created_at()->set_seconds(1700003600);
    auto *older = log.add_commits();
    older->set_commit_id("c2");
    older->set_commit_hash("h2");
    older->set_parent_1_id("c0");
    older->set_author_name("Bob");
    older->set_message("older commit");
    older->mutable_created_at()->set_seconds(1700000000);
    daemon.service().commitLogs["/work/assets"] = log;

    greet::GetCommitResponse detail;
    auto *commit = detail.mutable_commit();
    commit->set_commit_id("c1");
    commit->set_commit_hash("h1");
    commit->set_parent_1_id("c0");
    commit->set_message("newest commit");
    commit->mutable_created_at()->set_seconds(1700003600);
    auto *root = detail.mutable_root_tree();
    root->set_path("root");
    auto *file = root->add_files();
    file->set_path("a.txt");
    daemon.service().commitDetails["c1"] = detail;

    greet::FileLockDetail lock;
    lock.set_id("l1");
    lock.set_path("assets/logo.png");
    lock.set_global(true);
    lock.set_held_by_name("Maya");
    lock.mutable_acquired_at()->set_seconds(1700000000);
    daemon.service().locks.push_back(lock);

    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString configPath = tmp.path() + QStringLiteral("/nipa/daemon.json");
    QVERIFY(!writeDaemonFile(configPath, daemon.port(), QStringLiteral("token")).isEmpty());

    MainWindow window(nullptr, configPath, false);
    auto *connection = window.findChild<QLabel *>(QStringLiteral("connectionLabel"));
    QTRY_VERIFY_WITH_TIMEOUT(connection->text().contains(QStringLiteral("9.9.9")), 10000);
    window.openRepository(QStringLiteral("/work/assets"));

    auto *commitTable = window.findChild<QTableView *>(QStringLiteral("commitLogTable"));
    QVERIFY(commitTable != nullptr);
    QTRY_COMPARE(commitTable->model()->rowCount(), 2);

    auto *detailLabel = window.findChild<QLabel *>(QStringLiteral("commitDetailLabel"));
    QVERIFY(detailLabel != nullptr);
    QTRY_VERIFY(detailLabel->text().contains(QStringLiteral("c1")));
    QVERIFY(detailLabel->text().contains(QStringLiteral("Ada")));

    auto *commitTree = window.findChild<QTreeView *>(QStringLiteral("commitTreeView"));
    QVERIFY(commitTree != nullptr);
    QTRY_COMPARE(commitTree->model()->rowCount(), 1);

    auto *locksTable = window.findChild<QTableView *>(QStringLiteral("locksTable"));
    QVERIFY(locksTable != nullptr);
    QTRY_COMPARE(locksTable->model()->rowCount(), 1);
    QCOMPARE(locksTable->model()->index(0, 0).data().toString(),
             QStringLiteral("assets/logo.png"));
}

void MainWindowTest::diffsSelectedPendingFile()
{
    FakeDaemon daemon;
    nipadaemon::RepoInfo repo;
    repo.set_root("/work/assets");
    repo.set_branch("main");
    daemon.service().repos.push_back(repo);
    daemon.service().watchedRepos["/work/assets"] = repo;

    nipadaemon::StatusResponse status;
    status.set_branch("main");
    status.add_modified("textures/diffuse.png");
    daemon.service().statuses["/work/assets"] = status;

    daemon.service().diffScript.chunks = {
        "diff --git a/textures/diffuse.png b/textures/diffuse.png\n", "@@ -1 +1 @@\n", "-old\n",
        "+new\n"};

    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString configPath = tmp.path() + QStringLiteral("/nipa/daemon.json");
    QVERIFY(!writeDaemonFile(configPath, daemon.port(), QStringLiteral("token")).isEmpty());

    MainWindow window(nullptr, configPath, false);
    auto *connection = window.findChild<QLabel *>(QStringLiteral("connectionLabel"));
    QTRY_VERIFY_WITH_TIMEOUT(connection->text().contains(QStringLiteral("9.9.9")), 10000);
    window.openRepository(QStringLiteral("/work/assets"));

    auto *table = window.findChild<QTableView *>(QStringLiteral("statusTable"));
    QVERIFY(table != nullptr);
    QTRY_COMPARE(table->model()->rowCount(), 1);

    table->selectRow(0);
    auto *diffAction = window.findChild<QAction *>(QStringLiteral("diffAction"));
    QVERIFY(diffAction != nullptr);
    QTRY_VERIFY(diffAction->isEnabled());
    diffAction->trigger();

    auto *diffView = window.findChild<QPlainTextEdit *>(QStringLiteral("diffView"));
    QVERIFY(diffView != nullptr);
    QTRY_VERIFY(diffView->toPlainText().contains(QStringLiteral("+new")));

    QCOMPARE(daemon.service().diffRequests.size(), std::size_t(1));
    const nipadaemon::DiffRequest &request = daemon.service().diffRequests[0];
    QCOMPARE(request.paths_size(), 1);
    QCOMPARE(QString::fromStdString(request.paths(0)), QStringLiteral("textures/diffuse.png"));
    QVERIFY(!request.staged());

    // Refresh with changed toolbar options: staged set, ignore-all-space, context.
    auto *stagedCheck = window.findChild<QCheckBox *>(QStringLiteral("diffStagedCheck"));
    auto *whitespaceCombo = window.findChild<QComboBox *>(QStringLiteral("diffWhitespaceCombo"));
    auto *contextSpin = window.findChild<QSpinBox *>(QStringLiteral("diffContextSpin"));
    auto *refreshDiffButton = window.findChild<QPushButton *>(QStringLiteral("refreshDiffButton"));
    QVERIFY(stagedCheck != nullptr);
    QVERIFY(whitespaceCombo != nullptr);
    QVERIFY(contextSpin != nullptr);
    QVERIFY(refreshDiffButton != nullptr);

    stagedCheck->setChecked(true);
    whitespaceCombo->setCurrentIndex(whitespaceCombo->findData(QStringLiteral("all")));
    contextSpin->setValue(5);
    refreshDiffButton->click();

    QTRY_COMPARE(daemon.service().diffRequests.size(), std::size_t(2));
    const nipadaemon::DiffRequest &refreshed = daemon.service().diffRequests[1];
    QVERIFY(refreshed.staged());
    QVERIFY(refreshed.ignore_all_space());
    QCOMPARE(refreshed.context(), 5);
}

void MainWindowTest::showsBranchesAndGraph()
{
    FakeDaemon daemon;
    nipadaemon::RepoInfo repo;
    repo.set_root("/work/assets");
    repo.set_branch("main");
    daemon.service().repos.push_back(repo);
    daemon.service().watchedRepos["/work/assets"] = repo;

    nipadaemon::StatusResponse status;
    status.set_branch("main");
    daemon.service().statuses["/work/assets"] = status;

    greet::GetListBranchResponse list;
    auto *mainBranch = list.add_branches();
    mainBranch->set_id("b1");
    mainBranch->set_name("main");
    mainBranch->set_commit_id("c1");
    mainBranch->set_is_default(true);
    auto *feature = list.add_branches();
    feature->set_id("b2");
    feature->set_name("feature/x");
    feature->set_commit_id("c2");
    daemon.service().branchLists["/work/assets"] = list;

    greet::WalkCommitsResponse walk;
    auto *head = walk.add_commits();
    head->set_commit_id("c1");
    head->set_parent_1_id("c0");
    head->set_message("head commit");
    head->mutable_created_at()->set_seconds(1700003600);
    auto *parent = walk.add_commits();
    parent->set_commit_id("c0");
    parent->set_message("root commit");
    daemon.service().commitWalks["/work/assets"] = walk;

    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString configPath = tmp.path() + QStringLiteral("/nipa/daemon.json");
    QVERIFY(!writeDaemonFile(configPath, daemon.port(), QStringLiteral("token")).isEmpty());

    MainWindow window(nullptr, configPath, false);
    auto *connection = window.findChild<QLabel *>(QStringLiteral("connectionLabel"));
    QTRY_VERIFY_WITH_TIMEOUT(connection->text().contains(QStringLiteral("9.9.9")), 10000);
    window.openRepository(QStringLiteral("/work/assets"));

    auto *branchesTable = window.findChild<QTableView *>(QStringLiteral("branchesTable"));
    QVERIFY(branchesTable != nullptr);
    QTRY_COMPARE(branchesTable->model()->rowCount(), 2);

    auto *switchButton = window.findChild<QPushButton *>(QStringLiteral("switchBranchButton"));
    QVERIFY(switchButton != nullptr);
    // The active branch is selected and cannot be switched to itself.
    QVERIFY(!switchButton->isEnabled());
    branchesTable->selectRow(1);
    QTRY_VERIFY(switchButton->isEnabled());

    auto *graphTable = window.findChild<QTableView *>(QStringLiteral("graphTable"));
    QVERIFY(graphTable != nullptr);
    QTRY_COMPARE(graphTable->model()->rowCount(), 2);
    QVERIFY(graphTable->model()->index(0, 0).data().toString().contains(QChar(0x25CF)));

    auto *mergeAction = window.findChild<QAction *>(QStringLiteral("mergeAction"));
    QVERIFY(mergeAction != nullptr);
    QTRY_VERIFY(mergeAction->isEnabled());
}

QTEST_MAIN(MainWindowTest)

#include "mainwindow_test.moc"
