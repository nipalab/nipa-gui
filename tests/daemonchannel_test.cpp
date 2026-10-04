#include "daemon/daemonchannel.h"

#include "fakedaemon.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QThread>
#include <QtTest>

#include <memory>

namespace {

struct ConnectedFixture {
    QTemporaryDir tmp;
    std::unique_ptr<FakeDaemon> daemon;
    std::unique_ptr<DaemonChannel> channel;
};

ConnectedFixture makeConnectedFixture()
{
    ConnectedFixture fixture;
    fixture.daemon = std::make_unique<FakeDaemon>();
    const QString path = fixture.tmp.path() + QStringLiteral("/nipa/daemon.json");
    writeDaemonFile(path, fixture.daemon->port(), QStringLiteral("token"));
    fixture.channel = std::make_unique<DaemonChannel>(path);
    fixture.channel->setAutoSpawnEnabled(false);
    fixture.channel->start();

    QElapsedTimer timer;
    timer.start();
    while (!fixture.channel->isConnected() && timer.elapsed() < 10000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(5);
    }
    return fixture;
}

} // namespace

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
    void fetchTreeParsesManifest();
    void stageSendsAddAndUnstage();
    void pushOperationStreamsProgressAndResult();
    void updateOperationEmitsSyncResult();
    void failedOperationReportsFailure();
    void cancelsRunningOperation();
    void commitLogFetchesPages();
    void commitDetailIncludesTree();
    void diffStreamsChunks();
    void diffFailureReportsError();
    void diffCancels();
    void locksRoundTrip();
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

void DaemonChannelTest::fetchTreeParsesManifest()
{
    auto fixture = makeConnectedFixture();
    QVERIFY(fixture.channel->isConnected());

    greet::GetTreeManifestResponse manifest;
    manifest.set_branch("main");
    auto *root = manifest.mutable_root_tree();
    root->set_path("root");
    auto *readme = root->add_files();
    readme->set_path("README.md");
    readme->set_size_bytes(100);
    readme->set_mode(greet::FILE_MODE_READ_WRITE);
    auto *assets = root->add_sub_trees();
    assets->set_path("assets");
    auto *logo = assets->add_files();
    logo->set_path("logo.png");
    logo->set_size_bytes(2048);
    logo->set_is_binary(true);
    logo->set_mode(greet::FILE_MODE_READ_ONLY);
    fixture.daemon->service().treeManifests["/work/assets"] = manifest;

    QSignalSpy spy(fixture.channel.get(), &DaemonChannel::treeFetched);
    fixture.channel->fetchTree("/work/assets", "main", {"assets"});
    QTRY_COMPARE(spy.count(), 1);
    QVERIFY(spy.last().at(0).toBool());
    QCOMPARE(spy.last().at(1).toString(), QStringLiteral("/work/assets"));
    QCOMPARE(spy.last().at(2).toString(), QStringLiteral("main"));

    const TreeNodeData tree = spy.last().at(3).value<TreeNodeData>();
    QCOMPARE(tree.children.size(), 2);

    const auto findChild = [&tree](const QString &name) {
        for (const TreeNodeData &child : tree.children) {
            if (child.name == name) {
                return child;
            }
        }
        return TreeNodeData{};
    };
    const TreeNodeData assetsNode = findChild(QStringLiteral("assets"));
    QVERIFY(assetsNode.isDirectory);
    QCOMPARE(assetsNode.path, QStringLiteral("assets"));
    QCOMPARE(assetsNode.children.size(), 1);
    QCOMPARE(assetsNode.children.first().path, QStringLiteral("assets/logo.png"));
    QVERIFY(assetsNode.children.first().isBinary);
    QVERIFY(assetsNode.children.first().readOnly);
    QCOMPARE(assetsNode.children.first().sizeBytes, 2048);

    const TreeNodeData readmeNode = findChild(QStringLiteral("README.md"));
    QVERIFY(!readmeNode.isDirectory);
    QCOMPARE(readmeNode.path, QStringLiteral("README.md"));

    QVERIFY(fixture.daemon->service().lastTreeRequest.recursive());
    QCOMPARE(QString::fromStdString(fixture.daemon->service().lastTreeRoot), QStringLiteral("/work/assets"));
    QCOMPARE(fixture.daemon->service().lastTreeRequest.paths_size(), 1);
    QCOMPARE(QString::fromStdString(fixture.daemon->service().lastTreeRequest.paths(0)),
             QStringLiteral("assets"));
}

void DaemonChannelTest::stageSendsAddAndUnstage()
{
    auto fixture = makeConnectedFixture();
    QVERIFY(fixture.channel->isConnected());

    nipadaemon::StatusResponse status;
    status.add_staged("new.png");
    fixture.daemon->service().statuses["/work/assets"] = status;

    QSignalSpy spy(fixture.channel.get(), &DaemonChannel::staged);
    fixture.channel->stage("/work/assets", {"new.png", "assets/logo.png"}, {});
    QTRY_COMPARE(spy.count(), 1);
    QVERIFY(spy.last().at(0).toBool());
    QCOMPARE(spy.last().at(2).value<StatusSnapshot>().staged, QStringList{QStringLiteral("new.png")});

    QCOMPARE(fixture.daemon->service().stageRequests.size(), std::size_t(1));
    const nipadaemon::StageRequest &first = fixture.daemon->service().stageRequests[0];
    QCOMPARE(first.add_size(), 2);
    QCOMPARE(QString::fromStdString(first.add(0)), QStringLiteral("new.png"));
    QCOMPARE(QString::fromStdString(first.add(1)), QStringLiteral("assets/logo.png"));

    spy.clear();
    fixture.channel->stage("/work/assets", {}, {"new.png"});
    QTRY_COMPARE(spy.count(), 1);
    QCOMPARE(fixture.daemon->service().stageRequests.size(), std::size_t(2));
    const nipadaemon::StageRequest &second = fixture.daemon->service().stageRequests[1];
    QCOMPARE(second.unstage_size(), 1);
    QCOMPARE(QString::fromStdString(second.unstage(0)), QStringLiteral("new.png"));
}

void DaemonChannelTest::pushOperationStreamsProgressAndResult()
{
    auto fixture = makeConnectedFixture();
    QVERIFY(fixture.channel->isConnected());

    FakeStreamScript script;
    script.phase = "push";
    script.queuedAhead = 1;
    script.steps = 3;
    fixture.daemon->service().pushScript = script;

    DaemonOperation *operation = fixture.channel->push("/work/assets", QStringLiteral("add assets"));
    QSignalSpy queuedSpy(operation, &DaemonOperation::queued);
    QSignalSpy startedSpy(operation, &DaemonOperation::started);
    QSignalSpy progressSpy(operation, &DaemonOperation::progress);
    QSignalSpy finishedSpy(operation, &DaemonOperation::finished);

    QTRY_COMPARE(finishedSpy.count(), 1);
    QCOMPARE(queuedSpy.count(), 1);
    QCOMPARE(queuedSpy.last().at(0).toInt(), 1);
    QCOMPARE(startedSpy.count(), 1);
    QCOMPARE(startedSpy.last().at(0).toString(), QStringLiteral("push"));
    QCOMPARE(progressSpy.count(), 3);

    const OperationResult result = finishedSpy.last().at(0).value<OperationResult>();
    QVERIFY(result.kind == OperationResult::Push);
    QCOMPARE(result.push.commitId, QStringLiteral("pushed0001"));
    QCOMPARE(result.push.commitHash, QStringLiteral("hash0001"));

    QCOMPARE(fixture.daemon->service().pushRequests.size(), std::size_t(1));
    QCOMPARE(QString::fromStdString(fixture.daemon->service().pushRequests[0].message()),
             QStringLiteral("add assets"));
}

void DaemonChannelTest::updateOperationEmitsSyncResult()
{
    auto fixture = makeConnectedFixture();
    QVERIFY(fixture.channel->isConnected());

    DaemonOperation *operation = fixture.channel->update("/work/assets");
    QSignalSpy finishedSpy(operation, &DaemonOperation::finished);
    QTRY_COMPARE(finishedSpy.count(), 1);

    const OperationResult result = finishedSpy.last().at(0).value<OperationResult>();
    QVERIFY(result.kind == OperationResult::Sync);
    QCOMPARE(result.sync.branch, QStringLiteral("main"));
    QCOMPARE(result.sync.commitId, QStringLiteral("updated0001"));
    QCOMPARE(fixture.daemon->service().updateRequests.size(), std::size_t(1));
}

void DaemonChannelTest::failedOperationReportsFailure()
{
    auto fixture = makeConnectedFixture();
    QVERIFY(fixture.channel->isConnected());

    FakeStreamScript script;
    script.phase = "push";
    script.fail = true;
    script.failureCode = 409;
    script.failureMessage = "file is locked by someone else";
    fixture.daemon->service().pushScript = script;

    DaemonOperation *operation = fixture.channel->push("/work/assets", QStringLiteral("locked"));
    QSignalSpy failedSpy(operation, &DaemonOperation::failed);
    QSignalSpy finishedSpy(operation, &DaemonOperation::finished);

    QTRY_COMPARE(failedSpy.count(), 1);
    QCOMPARE(failedSpy.last().at(0).toInt(), 409);
    QCOMPARE(failedSpy.last().at(1).toString(), QStringLiteral("file is locked by someone else"));
    QCOMPARE(finishedSpy.count(), 0);
}

void DaemonChannelTest::cancelsRunningOperation()
{
    auto fixture = makeConnectedFixture();
    QVERIFY(fixture.channel->isConnected());

    FakeStreamScript script;
    script.phase = "push";
    script.steps = 100;
    script.stepDelayMs = 50;
    fixture.daemon->service().pushScript = script;

    DaemonOperation *operation = fixture.channel->push("/work/assets", QStringLiteral("slow"));
    QSignalSpy progressSpy(operation, &DaemonOperation::progress);
    QSignalSpy cancelledSpy(operation, &DaemonOperation::cancelled);
    QSignalSpy finishedSpy(operation, &DaemonOperation::finished);

    QTRY_VERIFY(progressSpy.count() >= 1);
    operation->cancel();
    QTRY_COMPARE(cancelledSpy.count(), 1);
    QCOMPARE(finishedSpy.count(), 0);
}

void DaemonChannelTest::commitLogFetchesPages()
{
    auto fixture = makeConnectedFixture();
    QVERIFY(fixture.channel->isConnected());

    greet::GetCommitLogResponse page;
    page.set_branch("main");
    auto *first = page.add_commits();
    first->set_commit_id("c1");
    first->set_commit_hash("h1");
    first->set_author_name("Ada");
    first->set_author_email("ada@example.com");
    first->set_message("one");
    first->mutable_created_at()->set_seconds(1700000000);
    auto *second = page.add_commits();
    second->set_commit_id("c2");
    second->set_commit_hash("h2");
    second->set_parent_1_id("c0");
    second->set_author_name("Bob");
    second->set_message("two");
    fixture.daemon->service().commitLogs["/work/assets"] = page;

    QSignalSpy spy(fixture.channel.get(), &DaemonChannel::commitLogFetched);
    fixture.channel->fetchCommitLog("/work/assets", "main", QString(), 50);
    QTRY_COMPARE(spy.count(), 1);
    QVERIFY(spy.last().at(0).toBool());
    QCOMPARE(spy.last().at(2).toString(), QStringLiteral("main"));

    const QList<CommitInfo> commits = spy.last().at(3).value<QList<CommitInfo>>();
    QCOMPARE(commits.size(), 2);
    QCOMPARE(commits.first().id, QStringLiteral("c1"));
    QCOMPARE(commits.first().authorName, QStringLiteral("Ada"));
    QCOMPARE(commits.first().message, QStringLiteral("one"));
    QVERIFY(commits.first().createdAt.isValid());
    QCOMPARE(commits.at(1).parent1, QStringLiteral("c0"));

    const auto &requests = fixture.daemon->service().commitLogRequests;
    QCOMPARE(requests.size(), std::size_t(1));
    QCOMPARE(requests[0].request().limit(), 50);
    QCOMPARE(QString::fromStdString(requests[0].request().branch()), QStringLiteral("main"));
    QVERIFY(!requests[0].request().has_start_commit_id());

    spy.clear();
    fixture.channel->fetchCommitLog("/work/assets", "main", QStringLiteral("c2"), 50);
    QTRY_COMPARE(spy.count(), 1);
    QCOMPARE(fixture.daemon->service().commitLogRequests.size(), std::size_t(2));
    const auto &secondRequest = fixture.daemon->service().commitLogRequests[1];
    QVERIFY(secondRequest.request().has_start_commit_id());
    QCOMPARE(QString::fromStdString(secondRequest.request().start_commit_id()), QStringLiteral("c2"));
}

void DaemonChannelTest::commitDetailIncludesTree()
{
    auto fixture = makeConnectedFixture();
    QVERIFY(fixture.channel->isConnected());

    greet::GetCommitResponse detail;
    auto *commit = detail.mutable_commit();
    commit->set_commit_id("c1");
    commit->set_commit_hash("h1");
    commit->set_parent_1_id("c0");
    commit->set_message("message");
    auto *root = detail.mutable_root_tree();
    root->set_path("root");
    auto *file = root->add_files();
    file->set_path("a.txt");
    file->set_size_bytes(12);
    fixture.daemon->service().commitDetails["c1"] = detail;

    QSignalSpy spy(fixture.channel.get(), &DaemonChannel::commitFetched);
    fixture.channel->fetchCommit("/work/assets", "c1");
    QTRY_COMPARE(spy.count(), 1);
    QVERIFY(spy.last().at(0).toBool());

    const CommitInfo fetched = spy.last().at(2).value<CommitInfo>();
    QCOMPARE(fetched.id, QStringLiteral("c1"));
    QCOMPARE(fetched.parent1, QStringLiteral("c0"));
    QCOMPARE(fetched.message, QStringLiteral("message"));

    const TreeNodeData tree = spy.last().at(3).value<TreeNodeData>();
    QCOMPARE(tree.children.size(), 1);
    QCOMPARE(tree.children.first().path, QStringLiteral("a.txt"));

    QCOMPARE(fixture.daemon->service().commitGetRequests.size(), std::size_t(1));
    QCOMPARE(fixture.daemon->service().commitGetRequests[0], std::string("c1"));
}

void DaemonChannelTest::diffStreamsChunks()
{
    auto fixture = makeConnectedFixture();
    QVERIFY(fixture.channel->isConnected());

    fixture.daemon->service().diffScript.chunks = {
        "diff --git a/a.txt b/a.txt\n", "@@ -1 +1 @@\n", "-old\n", "+new\n"};

    DiffRequestData request;
    request.paths = {QStringLiteral("a.txt")};
    DaemonDiff *stream = fixture.channel->diff("/work/assets", request);
    QSignalSpy dataSpy(stream, &DaemonDiff::dataReceived);
    QSignalSpy finishedSpy(stream, &DaemonDiff::finished);
    QSignalSpy failedSpy(stream, &DaemonDiff::failed);

    QTRY_COMPARE(finishedSpy.count(), 1);
    QCOMPARE(failedSpy.count(), 0);

    QByteArray combined;
    for (const QList<QVariant> &emission : dataSpy) {
        combined += emission.at(0).toByteArray();
    }
    QCOMPARE(combined, QByteArray("diff --git a/a.txt b/a.txt\n@@ -1 +1 @@\n-old\n+new\n"));

    QCOMPARE(fixture.daemon->service().diffRequests.size(), std::size_t(1));
    const nipadaemon::DiffRequest &sent = fixture.daemon->service().diffRequests[0];
    QCOMPARE(QString::fromStdString(sent.format()), QStringLiteral("patch"));
    QCOMPARE(sent.paths_size(), 1);
    QCOMPARE(QString::fromStdString(sent.paths(0)), QStringLiteral("a.txt"));
    QVERIFY(!sent.staged());
}

void DaemonChannelTest::diffFailureReportsError()
{
    auto fixture = makeConnectedFixture();
    QVERIFY(fixture.channel->isConnected());

    fixture.daemon->service().diffScript.fail = true;
    fixture.daemon->service().diffScript.failureCode = 400;
    fixture.daemon->service().diffScript.failureMessage = "unknown diff format";

    DaemonDiff *stream = fixture.channel->diff("/work/assets", {});
    QSignalSpy failedSpy(stream, &DaemonDiff::failed);
    QSignalSpy finishedSpy(stream, &DaemonDiff::finished);

    QTRY_COMPARE(failedSpy.count(), 1);
    QCOMPARE(failedSpy.last().at(0).toInt(), 400);
    QCOMPARE(failedSpy.last().at(1).toString(), QStringLiteral("unknown diff format"));
    QCOMPARE(finishedSpy.count(), 0);
}

void DaemonChannelTest::diffCancels()
{
    auto fixture = makeConnectedFixture();
    QVERIFY(fixture.channel->isConnected());

    for (int i = 0; i < 100; ++i) {
        fixture.daemon->service().diffScript.chunks.push_back("chunk line\n");
    }
    fixture.daemon->service().diffScript.chunkDelayMs = 50;

    DaemonDiff *stream = fixture.channel->diff("/work/assets", {});
    QSignalSpy dataSpy(stream, &DaemonDiff::dataReceived);
    QSignalSpy cancelledSpy(stream, &DaemonDiff::cancelled);
    QSignalSpy finishedSpy(stream, &DaemonDiff::finished);

    QTRY_VERIFY(dataSpy.count() >= 1);
    stream->cancel();
    QTRY_COMPARE(cancelledSpy.count(), 1);
    QCOMPARE(finishedSpy.count(), 0);
}

void DaemonChannelTest::locksRoundTrip()
{
    auto fixture = makeConnectedFixture();
    QVERIFY(fixture.channel->isConnected());

    greet::FileLockDetail existing;
    existing.set_id("l1");
    existing.set_path("assets/logo.png");
    existing.set_global(true);
    existing.set_held_by("u2");
    existing.set_held_by_name("Maya");
    existing.mutable_acquired_at()->set_seconds(1700000000);
    fixture.daemon->service().locks.push_back(existing);

    QSignalSpy listSpy(fixture.channel.get(), &DaemonChannel::locksFetched);
    fixture.channel->fetchLocks("/work/assets");
    QTRY_COMPARE(listSpy.count(), 1);
    QVERIFY(listSpy.last().at(0).toBool());

    const QList<FileLockInfo> locks = listSpy.last().at(2).value<QList<FileLockInfo>>();
    QCOMPARE(locks.size(), 1);
    QCOMPARE(locks.first().path, QStringLiteral("assets/logo.png"));
    QVERIFY(locks.first().global);
    QCOMPARE(locks.first().heldByName, QStringLiteral("Maya"));
    QVERIFY(locks.first().acquiredAt.isValid());

    QSignalSpy lockSpy(fixture.channel.get(), &DaemonChannel::fileLocked);
    fixture.channel->lockFile("/work/assets", QStringLiteral("assets/tex.png"),
                              QStringLiteral("feature/x"));
    QTRY_COMPARE(lockSpy.count(), 1);
    QVERIFY(lockSpy.last().at(0).toBool());
    QCOMPARE(lockSpy.last().at(2).value<FileLockInfo>().path, QStringLiteral("assets/tex.png"));
    QCOMPARE(fixture.daemon->service().lockRequests.size(), std::size_t(1));
    QCOMPARE(QString::fromStdString(fixture.daemon->service().lockRequests[0].request().branch()),
             QStringLiteral("feature/x"));

    QSignalSpy unlockSpy(fixture.channel.get(), &DaemonChannel::fileUnlocked);
    fixture.channel->unlockFile("/work/assets", QStringLiteral("assets/tex.png"),
                                QStringLiteral("feature/x"));
    QTRY_COMPARE(unlockSpy.count(), 1);
    QVERIFY(unlockSpy.last().at(0).toBool());
    QCOMPARE(unlockSpy.last().at(2).toString(), QStringLiteral("assets/tex.png"));
    QCOMPARE(fixture.daemon->service().unlockRequests.size(), std::size_t(1));
    QCOMPARE(fixture.daemon->service().locks.size(), std::size_t(1));
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
