#include "ui/mainwindow.h"

#include "fakedaemon.h"

#include <QLabel>
#include <QTableView>
#include <QTemporaryDir>
#include <QtTest>

class MainWindowTest : public QObject {
    Q_OBJECT

private slots:
    void showsDisconnectedStatus();
    void reconnectsAfterDaemonAppears();
    void showsRepositoryStatus();
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

    QCOMPARE(daemon.service().watchedRoots.size(), std::size_t(1));
    QCOMPARE(daemon.service().statusRequests.size(), std::size_t(1));
}

QTEST_MAIN(MainWindowTest)

#include "mainwindow_test.moc"
