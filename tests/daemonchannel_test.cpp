#include "daemon/daemonchannel.h"

#include "fakedaemon.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class DaemonChannelTest : public QObject {
    Q_OBJECT

private slots:
    void asyncPingConnectsWithToken();
    void rejectedRequestReportsError();
    void missingConfigWithoutAutoSpawnReportsError();
    void autoSpawnUnavailableReportsError();
    void autoSpawnWaitsForDaemonFile();
    void listReposAndWatchFlow();
    void fetchStatusMapsAllCategories();
    void reconnectsAfterDaemonAppears();
    void versionComparison();
};

void DaemonChannelTest::asyncPingConnectsWithToken()
{
    FakeDaemon daemon;
    QVERIFY(daemon.port() > 0);

    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = tmp.path() + QStringLiteral("/nipa/daemon.json");
    QVERIFY(!writeDaemonFile(path, daemon.port(), QStringLiteral("sekret")).isEmpty());

    DaemonChannel channel(path);
    channel.setAutoSpawnEnabled(false);
    QSignalSpy connectedSpy(&channel, &DaemonChannel::connected);

    channel.start();
    QTRY_VERIFY_WITH_TIMEOUT(channel.isConnected(), 10000);
    QCOMPARE(connectedSpy.count(), 1);
    QCOMPARE(channel.status().version, QStringLiteral("9.9.9"));
    QCOMPARE(channel.status().pid, 4242);
    QCOMPARE(channel.status().endpoint, QStringLiteral("127.0.0.1:") + QString::number(daemon.port()));
    QCOMPARE(QString::fromStdString(daemon.service().token), QStringLiteral("sekret"));
}

void DaemonChannelTest::rejectedRequestReportsError()
{
    FakeDaemon daemon;
    daemon.service().requiredToken = "expected";

    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = tmp.path() + QStringLiteral("/nipa/daemon.json");
    QVERIFY(!writeDaemonFile(path, daemon.port(), QStringLiteral("wrong")).isEmpty());

    DaemonChannel channel(path);
    channel.setAutoSpawnEnabled(false);
    QSignalSpy errorSpy(&channel, &DaemonChannel::errorOccurred);

    channel.start();
    QTRY_VERIFY(errorSpy.count() > 0);
    QVERIFY2(errorSpy.last().at(0).toString().contains(QStringLiteral("invalid token")),
             qPrintable(errorSpy.last().at(0).toString()));
    QVERIFY(!channel.isConnected());
    QVERIFY(channel.state() == DaemonChannel::State::Failed);
}

void DaemonChannelTest::missingConfigWithoutAutoSpawnReportsError()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    DaemonChannel channel(tmp.path() + QStringLiteral("/missing/daemon.json"));
    channel.setAutoSpawnEnabled(false);
    QSignalSpy errorSpy(&channel, &DaemonChannel::errorOccurred);

    channel.start();
    QTRY_VERIFY(errorSpy.count() > 0);
    QVERIFY(errorSpy.last().at(0).toString().contains(QStringLiteral("cannot read")));
    QVERIFY(channel.state() == DaemonChannel::State::Disconnected);
}

void DaemonChannelTest::autoSpawnUnavailableReportsError()
{
    const QByteArray oldPath = qgetenv("PATH");
    qputenv("PATH", QByteArray());

    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    DaemonChannel channel(tmp.path() + QStringLiteral("/nipa/daemon.json"));
    QSignalSpy errorSpy(&channel, &DaemonChannel::errorOccurred);

    channel.start();
    QTRY_VERIFY(errorSpy.count() > 0);
    QVERIFY2(errorSpy.last().at(0).toString().contains(QStringLiteral("was not found in PATH")),
             qPrintable(errorSpy.last().at(0).toString()));
    QVERIFY(!channel.ownsDaemonProcess());

    qputenv("PATH", oldPath);
}

void DaemonChannelTest::autoSpawnWaitsForDaemonFile()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString binDir = tmp.path() + QStringLiteral("/bin");
    QVERIFY(QDir().mkpath(binDir));
    const QString script = binDir + QStringLiteral("/nipa");
    QFile file(script);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("#!/bin/sh\n: > \"${0%/*}/spawned\"\nexec /bin/sleep 15\n");
    file.close();
    QVERIFY(file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
                                | QFileDevice::ReadGroup | QFileDevice::ExeGroup
                                | QFileDevice::ReadOther | QFileDevice::ExeOther));

    const QByteArray oldPath = qgetenv("PATH");
    qputenv("PATH", binDir.toUtf8());

    const QString configPath = tmp.path() + QStringLiteral("/nipa/daemon.json");
    FakeDaemon daemon;
    DaemonChannel channel(configPath);
    QSignalSpy errorSpy(&channel, &DaemonChannel::errorOccurred);

    channel.start();
    QVERIFY(channel.ownsDaemonProcess());
    QTRY_VERIFY(QFile::exists(binDir + QStringLiteral("/spawned")));

    // The spawned process publishes the endpoint only now; the readiness poll
    // must pick it up without another spawn.
    QVERIFY(!writeDaemonFile(configPath, daemon.port(), QStringLiteral("token")).isEmpty());
    QTRY_VERIFY_WITH_TIMEOUT(channel.isConnected(), 10000);
    QCOMPARE(errorSpy.count(), 0);

    channel.shutdownIfOwned();
    QVERIFY(!channel.ownsDaemonProcess());
    QCOMPARE(errorSpy.count(), 0);

    qputenv("PATH", oldPath);
}

void DaemonChannelTest::listReposAndWatchFlow()
{
    FakeDaemon daemon;
    nipadaemon::RepoInfo repo;
    repo.set_root("/work/assets");
    repo.set_url("https://nipa.example/org/proj");
    repo.set_branch("main");
    repo.add_sparse("assets");
    daemon.service().repos.push_back(repo);
    daemon.service().watchedRepos["/work/assets"] = repo;

    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = tmp.path() + QStringLiteral("/nipa/daemon.json");
    QVERIFY(!writeDaemonFile(path, daemon.port(), QStringLiteral("token")).isEmpty());

    DaemonChannel channel(path);
    channel.setAutoSpawnEnabled(false);
    channel.start();
    QTRY_VERIFY_WITH_TIMEOUT(channel.isConnected(), 10000);

    QSignalSpy reposSpy(&channel, &DaemonChannel::reposListed);
    channel.listRepos();
    QTRY_COMPARE(reposSpy.count(), 1);
    QVERIFY(reposSpy.last().at(0).toBool());
    const QList<RepoInfo> repos = reposSpy.last().at(1).value<QList<RepoInfo>>();
    QCOMPARE(repos.size(), 1);
    QCOMPARE(repos.first().root, QStringLiteral("/work/assets"));
    QCOMPARE(repos.first().url, QStringLiteral("https://nipa.example/org/proj"));
    QCOMPARE(repos.first().branch, QStringLiteral("main"));
    QCOMPARE(repos.first().sparse, QStringList{QStringLiteral("assets")});

    QSignalSpy watchSpy(&channel, &DaemonChannel::repoWatched);
    channel.watchRepo("/work/assets");
    QTRY_COMPARE(watchSpy.count(), 1);
    QVERIFY(watchSpy.last().at(0).toBool());
    QCOMPARE(watchSpy.last().at(1).value<RepoInfo>().root, QStringLiteral("/work/assets"));
    QCOMPARE(daemon.service().watchedRoots.size(), std::size_t(1));

    QSignalSpy unwatchSpy(&channel, &DaemonChannel::repoUnwatched);
    channel.unwatchRepo("/work/assets");
    QTRY_COMPARE(unwatchSpy.count(), 1);
    QVERIFY(unwatchSpy.last().at(0).toBool());
    QCOMPARE(daemon.service().unwatchedRoots.size(), std::size_t(1));
}

void DaemonChannelTest::fetchStatusMapsAllCategories()
{
    FakeDaemon daemon;
    nipadaemon::StatusResponse status;
    status.add_staged("new.png");
    status.add_deleted("old.bin");
    status.add_modified("tex/diffuse.png");
    status.add_untracked("wip.txt");
    status.add_missing("gone.dat");
    status.add_conflicts("merge.bin");
    status.set_branch("feature/x");
    status.mutable_head()->set_kind("tag");
    status.mutable_head()->set_name("v1.2.3");
    daemon.service().statuses["/work/assets"] = status;

    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = tmp.path() + QStringLiteral("/nipa/daemon.json");
    QVERIFY(!writeDaemonFile(path, daemon.port(), QStringLiteral("token")).isEmpty());

    DaemonChannel channel(path);
    channel.setAutoSpawnEnabled(false);
    channel.start();
    QTRY_VERIFY_WITH_TIMEOUT(channel.isConnected(), 10000);

    QSignalSpy statusSpy(&channel, &DaemonChannel::statusFetched);
    channel.fetchStatus("/work/assets", true);
    QTRY_COMPARE(statusSpy.count(), 1);
    QVERIFY(statusSpy.last().at(0).toBool());
    QCOMPARE(statusSpy.last().at(1).toString(), QStringLiteral("/work/assets"));

    const StatusSnapshot snapshot = statusSpy.last().at(2).value<StatusSnapshot>();
    QCOMPARE(snapshot.staged, QStringList{QStringLiteral("new.png")});
    QCOMPARE(snapshot.deleted, QStringList{QStringLiteral("old.bin")});
    QCOMPARE(snapshot.modified, QStringList{QStringLiteral("tex/diffuse.png")});
    QCOMPARE(snapshot.untracked, QStringList{QStringLiteral("wip.txt")});
    QCOMPARE(snapshot.missing, QStringList{QStringLiteral("gone.dat")});
    QCOMPARE(snapshot.conflicts, QStringList{QStringLiteral("merge.bin")});
    QCOMPARE(snapshot.branch, QStringLiteral("feature/x"));
    QVERIFY(snapshot.detached());
    QCOMPARE(snapshot.headKind, QStringLiteral("tag"));
    QCOMPARE(snapshot.headName, QStringLiteral("v1.2.3"));
    QCOMPARE(snapshot.total(), 6);
    QVERIFY(daemon.service().lastStatusNoCache);
}

void DaemonChannelTest::reconnectsAfterDaemonAppears()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = tmp.path() + QStringLiteral("/nipa/daemon.json");

    DaemonChannel channel(path);
    channel.setAutoSpawnEnabled(false);
    channel.start();
    QVERIFY(!channel.isConnected());

    FakeDaemon daemon;
    QVERIFY(!writeDaemonFile(path, daemon.port(), QStringLiteral("token")).isEmpty());

    channel.reconnect();
    QTRY_VERIFY_WITH_TIMEOUT(channel.isConnected(), 10000);
}

void DaemonChannelTest::versionComparison()
{
    QCOMPARE(DaemonChannel::compareVersions(QStringLiteral("1.2.3"), QStringLiteral("1.2.3")), 0);
    QCOMPARE(DaemonChannel::compareVersions(QStringLiteral("1.10.0"), QStringLiteral("1.9.9")), 1);
    QCOMPARE(DaemonChannel::compareVersions(QStringLiteral("v2.0"), QStringLiteral("1.9.9")), 1);
    QCOMPARE(DaemonChannel::compareVersions(QStringLiteral("1.2"), QStringLiteral("1.2.1")), -1);
    QCOMPARE(DaemonChannel::compareVersions(QStringLiteral("2.0.0-beta.1"), QStringLiteral("2.0.0")), 0);
    QCOMPARE(DaemonChannel::compareVersions(QString(), QStringLiteral("1.0")), -1);
}

QTEST_GUILESS_MAIN(DaemonChannelTest)

#include "daemonchannel_test.moc"
