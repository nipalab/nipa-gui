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
    void branchesCrud();
    void switchOperation();
    void mergeOperationWithConflicts();
    void commitWalkParses();
    void mergeRequestsFlow();
    void reviewsStateAndThreads();
    void revertOperationWithResume();
    void loginFlow();
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

void DaemonChannelTest::branchesCrud()
{
    auto fixture = makeConnectedFixture();
    QVERIFY(fixture.channel->isConnected());

    greet::GetListBranchResponse list;
    auto *mainBranch = list.add_branches();
    mainBranch->set_id("b1");
    mainBranch->set_name("main");
    mainBranch->set_commit_id("c1");
    mainBranch->set_is_default(true);
    auto *feature = list.add_branches();
    feature->set_id("b2");
    feature->set_name("feature");
    feature->set_is_protected(true);
    fixture.daemon->service().branchLists["/work/assets"] = list;

    QSignalSpy listSpy(fixture.channel.get(), &DaemonChannel::branchesFetched);
    fixture.channel->fetchBranches("/work/assets");
    QTRY_COMPARE(listSpy.count(), 1);
    QVERIFY(listSpy.last().at(0).toBool());

    const QList<BranchInfo> branches = listSpy.last().at(2).value<QList<BranchInfo>>();
    QCOMPARE(branches.size(), 2);
    QCOMPARE(branches.first().name, QStringLiteral("main"));
    QCOMPARE(branches.first().commitId, QStringLiteral("c1"));
    QVERIFY(branches.first().isDefault);
    QVERIFY(branches.at(1).isProtected);
    QCOMPARE(branches.at(1).commitId, QString());

    QSignalSpy createSpy(fixture.channel.get(), &DaemonChannel::branchCreated);
    fixture.channel->createBranch("/work/assets", QStringLiteral("feature/new"),
                                  QStringLiteral("main"));
    QTRY_COMPARE(createSpy.count(), 1);
    QVERIFY(createSpy.last().at(0).toBool());
    const BranchInfo created = createSpy.last().at(2).value<BranchInfo>();
    QCOMPARE(created.name, QStringLiteral("feature/new"));
    QCOMPARE(created.commitId, QStringLiteral("c1"));
    QCOMPARE(fixture.daemon->service().branchCreateRequests.size(), std::size_t(1));

    QSignalSpy deleteSpy(fixture.channel.get(), &DaemonChannel::branchDeleted);
    fixture.channel->deleteBranch("/work/assets", QStringLiteral("feature"));
    QTRY_COMPARE(deleteSpy.count(), 1);
    QVERIFY(deleteSpy.last().at(0).toBool());
    QCOMPARE(deleteSpy.last().at(2).toString(), QStringLiteral("feature"));
    QCOMPARE(fixture.daemon->service().branchDeleteRequests.size(), std::size_t(1));
    QCOMPARE(fixture.daemon->service().branchLists["/work/assets"].branches_size(), 2);
}

void DaemonChannelTest::switchOperation()
{
    auto fixture = makeConnectedFixture();
    QVERIFY(fixture.channel->isConnected());

    DaemonOperation *operation =
        fixture.channel->switchBranch("/work/assets", QStringLiteral("feature/x"));
    QSignalSpy finishedSpy(operation, &DaemonOperation::finished);
    QTRY_COMPARE(finishedSpy.count(), 1);

    const OperationResult result = finishedSpy.last().at(0).value<OperationResult>();
    QVERIFY(result.kind == OperationResult::Sync);
    QCOMPARE(result.sync.branch, QStringLiteral("feature/x"));
    QCOMPARE(result.sync.commitId, QStringLiteral("switched0001"));

    QCOMPARE(fixture.daemon->service().switchRequests.size(), std::size_t(1));
    QCOMPARE(QString::fromStdString(fixture.daemon->service().switchRequests[0].branch()),
             QStringLiteral("feature/x"));
}

void DaemonChannelTest::mergeOperationWithConflicts()
{
    auto fixture = makeConnectedFixture();
    QVERIFY(fixture.channel->isConnected());

    fixture.daemon->service().mergeCommitted = true;
    fixture.daemon->service().mergeConflicts = {"assets/a.png", "assets/b.png"};

    DaemonOperation *operation = fixture.channel->merge(
        "/work/assets", QStringLiteral("feature/x"), false, false, true, QStringLiteral("merge it"));
    QSignalSpy finishedSpy(operation, &DaemonOperation::finished);
    QTRY_COMPARE(finishedSpy.count(), 1);

    const OperationResult result = finishedSpy.last().at(0).value<OperationResult>();
    QVERIFY(result.kind == OperationResult::Merge);
    QVERIFY(result.merge.mergeCommitted);
    QCOMPARE(result.merge.conflicts.size(), 2);
    QCOMPARE(result.merge.conflicts.first(), QStringLiteral("assets/a.png"));

    QCOMPARE(fixture.daemon->service().mergeRequests.size(), std::size_t(1));
    const nipadaemon::MergeOpRequest &sent = fixture.daemon->service().mergeRequests[0];
    QCOMPARE(QString::fromStdString(sent.source_branch()), QStringLiteral("feature/x"));
    QVERIFY(sent.no_ff());
    QVERIFY(!sent.ff_only());
    QCOMPARE(QString::fromStdString(sent.message()), QStringLiteral("merge it"));

    fixture.daemon->service().mergeConflicts.clear();
    DaemonOperation *abortOperation = fixture.channel->merge(
        "/work/assets", QStringLiteral("feature/x"), true, false, false, QString());
    QSignalSpy abortSpy(abortOperation, &DaemonOperation::finished);
    QTRY_COMPARE(abortSpy.count(), 1);
    const OperationResult abortResult = abortSpy.last().at(0).value<OperationResult>();
    QVERIFY(abortResult.merge.aborted);
    QCOMPARE(fixture.daemon->service().mergeRequests.size(), std::size_t(2));
    QVERIFY(fixture.daemon->service().mergeRequests[1].abort());
}

void DaemonChannelTest::commitWalkParses()
{
    auto fixture = makeConnectedFixture();
    QVERIFY(fixture.channel->isConnected());

    greet::WalkCommitsResponse walk;
    auto *merge = walk.add_commits();
    merge->set_commit_id("m1");
    merge->set_parent_1_id("a1");
    merge->set_parent_2_id("b1");
    merge->set_message("merge feature");
    merge->mutable_created_at()->set_seconds(1700003600);
    auto *first = walk.add_commits();
    first->set_commit_id("a1");
    first->set_parent_1_id("base");
    first->set_message("on main");
    fixture.daemon->service().commitWalks["/work/assets"] = walk;

    QSignalSpy spy(fixture.channel.get(), &DaemonChannel::commitWalkFetched);
    fixture.channel->fetchCommitWalk("/work/assets", QStringLiteral("m1"), 200);
    QTRY_COMPARE(spy.count(), 1);
    QVERIFY(spy.last().at(0).toBool());

    const QList<CommitInfo> commits = spy.last().at(2).value<QList<CommitInfo>>();
    QCOMPARE(commits.size(), 2);
    QCOMPARE(commits.first().id, QStringLiteral("m1"));
    QCOMPARE(commits.first().parent1, QStringLiteral("a1"));
    QCOMPARE(commits.first().parent2, QStringLiteral("b1"));
    QCOMPARE(commits.at(1).parent1, QStringLiteral("base"));
    QVERIFY(commits.first().createdAt.isValid());

    QCOMPARE(fixture.daemon->service().commitWalkRequests.size(), std::size_t(1));
    const auto &sent = fixture.daemon->service().commitWalkRequests[0].request();
    QCOMPARE(QString::fromStdString(sent.start_commit_id()), QStringLiteral("m1"));
    QCOMPARE(sent.limit(), 200);
}

void DaemonChannelTest::mergeRequestsFlow()
{
    auto fixture = makeConnectedFixture();
    QVERIFY(fixture.channel->isConnected());

    greet::ListMergeRequestsResponse list;
    auto *mergeRequest = list.add_merge_requests();
    mergeRequest->set_id("mr-1");
    mergeRequest->set_number(1);
    mergeRequest->set_source_branch("feature");
    mergeRequest->set_target_branch("main");
    mergeRequest->set_title("Add feature");
    mergeRequest->set_status("open");
    mergeRequest->set_created_by("u1");
    mergeRequest->mutable_created_at()->set_seconds(1700000000);
    fixture.daemon->service().mergeRequestLists["/work/assets"] = list;

    QSignalSpy listSpy(fixture.channel.get(), &DaemonChannel::mergeRequestsFetched);
    fixture.channel->fetchMergeRequests("/work/assets", QStringLiteral("open"), 100);
    QTRY_COMPARE(listSpy.count(), 1);
    QVERIFY(listSpy.last().at(0).toBool());

    const QList<MergeRequestInfo> mergeRequests = listSpy.last().at(2).value<QList<MergeRequestInfo>>();
    QCOMPARE(mergeRequests.size(), 1);
    QCOMPARE(mergeRequests.first().number, qint64(1));
    QCOMPARE(mergeRequests.first().title, QStringLiteral("Add feature"));
    QCOMPARE(mergeRequests.first().status, QStringLiteral("open"));
    QVERIFY(mergeRequests.first().createdAt.isValid());

    QCOMPARE(fixture.daemon->service().mrListRequests.size(), std::size_t(1));
    QCOMPARE(QString::fromStdString(fixture.daemon->service().mrListRequests[0].request().status()),
             QStringLiteral("open"));
    QCOMPARE(fixture.daemon->service().mrListRequests[0].request().limit(), 100);

    QSignalSpy createSpy(fixture.channel.get(), &DaemonChannel::mergeRequestCreated);
    fixture.channel->createMergeRequest("/work/assets", QStringLiteral("New MR"),
                                        QStringLiteral("desc"), QStringLiteral("feature"),
                                        QStringLiteral("main"));
    QTRY_COMPARE(createSpy.count(), 1);
    QVERIFY(createSpy.last().at(0).toBool());
    QCOMPARE(createSpy.last().at(2).value<MergeRequestInfo>().number, qint64(2));
    QCOMPARE(fixture.daemon->service().mrCreateRequests.size(), std::size_t(1));

    QSignalSpy mergeSpy(fixture.channel.get(), &DaemonChannel::mergeRequestMerged);
    fixture.channel->mergeMergeRequest("/work/assets", 1);
    QTRY_COMPARE(mergeSpy.count(), 1);
    QVERIFY(mergeSpy.last().at(0).toBool());
    QCOMPARE(mergeSpy.last().at(2).value<MergeRequestInfo>().status, QStringLiteral("merged"));
    QCOMPARE(fixture.daemon->service().mrMergeRequests.size(), std::size_t(1));

    QSignalSpy closeSpy(fixture.channel.get(), &DaemonChannel::mergeRequestClosed);
    fixture.channel->closeMergeRequest("/work/assets", 2);
    QTRY_COMPARE(closeSpy.count(), 1);
    QVERIFY(closeSpy.last().at(0).toBool());
    QCOMPARE(closeSpy.last().at(2).value<MergeRequestInfo>().status, QStringLiteral("closed"));
    QCOMPARE(fixture.daemon->service().mrCloseRequests.size(), std::size_t(1));
}

void DaemonChannelTest::reviewsStateAndThreads()
{
    auto fixture = makeConnectedFixture();
    QVERIFY(fixture.channel->isConnected());

    greet::ListMergeRequestReviewsResponse reviews;
    auto *review = reviews.add_reviews();
    review->set_id("r1");
    review->set_state("approved");
    review->set_body("lgtm");
    review->mutable_reviewer()->set_user_id("u2");
    review->mutable_reviewer()->set_name("Maya");
    fixture.daemon->service().mergeRequestReviews[1] = reviews;

    greet::GetMergeRequestReviewStateResponse state;
    state.mutable_state()->set_approvals(1);
    state.mutable_state()->set_changes_requested(0);
    state.mutable_state()->add_outstanding_reviewers("u3");
    fixture.daemon->service().mergeRequestReviewStates[1] = state;

    greet::ListMergeRequestThreadsResponse threads;
    auto *thread = threads.add_threads();
    thread->set_id("t1");
    thread->set_file_path("a.png");
    thread->set_new_line(3);
    thread->set_side("right");
    thread->mutable_created_by()->set_name("Ada");
    auto *comment = thread->add_comments();
    comment->set_id("c1");
    comment->mutable_user()->set_name("Ada");
    comment->set_body("why?");
    fixture.daemon->service().mergeRequestThreads[1] = threads;

    QSignalSpy reviewsSpy(fixture.channel.get(), &DaemonChannel::mergeRequestReviewsFetched);
    QSignalSpy stateSpy(fixture.channel.get(), &DaemonChannel::mergeRequestReviewStateFetched);
    QSignalSpy threadsSpy(fixture.channel.get(), &DaemonChannel::mergeRequestThreadsFetched);

    fixture.channel->fetchMergeRequestReviews("/work/assets", 1);
    fixture.channel->fetchMergeRequestReviewState("/work/assets", 1);
    fixture.channel->fetchMergeRequestThreads("/work/assets", 1);
    QTRY_COMPARE(reviewsSpy.count(), 1);
    QTRY_COMPARE(stateSpy.count(), 1);
    QTRY_COMPARE(threadsSpy.count(), 1);

    const QList<ReviewInfo> fetchedReviews = reviewsSpy.last().at(3).value<QList<ReviewInfo>>();
    QCOMPARE(fetchedReviews.size(), 1);
    QCOMPARE(fetchedReviews.first().state, QStringLiteral("approved"));
    QCOMPARE(fetchedReviews.first().reviewer.name, QStringLiteral("Maya"));

    const ReviewStateInfo fetchedState = stateSpy.last().at(3).value<ReviewStateInfo>();
    QCOMPARE(fetchedState.approvals, 1);
    QCOMPARE(fetchedState.outstandingReviewers, QStringList{QStringLiteral("u3")});

    const QList<ReviewThreadInfo> fetchedThreads = threadsSpy.last().at(3).value<QList<ReviewThreadInfo>>();
    QCOMPARE(fetchedThreads.size(), 1);
    QCOMPARE(fetchedThreads.first().filePath, QStringLiteral("a.png"));
    QVERIFY(fetchedThreads.first().hasNewLine);
    QCOMPARE(fetchedThreads.first().newLine, qint64(3));
    QCOMPARE(fetchedThreads.first().comments.size(), 1);
    QCOMPARE(fetchedThreads.first().comments.first().body, QStringLiteral("why?"));

    QCOMPARE(fixture.daemon->service().mrReviewsRequests.size(), std::size_t(1));
    QCOMPARE(fixture.daemon->service().mrReviewStateRequests.size(), std::size_t(1));
    QCOMPARE(fixture.daemon->service().mrThreadsRequests.size(), std::size_t(1));
}

void DaemonChannelTest::revertOperationWithResume()
{
    auto fixture = makeConnectedFixture();
    QVERIFY(fixture.channel->isConnected());

    fixture.daemon->service().revertConflicts = {"a.txt"};

    DaemonOperation *operation = fixture.channel->revert(
        "/work/assets", QStringLiteral("c1"), false, false, false, false, 0, QStringLiteral("revert it"));
    QSignalSpy finishedSpy(operation, &DaemonOperation::finished);
    QTRY_COMPARE(finishedSpy.count(), 1);

    const OperationResult result = finishedSpy.last().at(0).value<OperationResult>();
    QVERIFY(result.kind == OperationResult::Revert);
    QCOMPARE(result.revert.conflicts.size(), 1);
    QCOMPARE(result.revert.conflicts.first(), QStringLiteral("a.txt"));

    QCOMPARE(fixture.daemon->service().revertRequests.size(), std::size_t(1));
    const nipadaemon::RevertOpRequest &sent = fixture.daemon->service().revertRequests[0];
    QCOMPARE(QString::fromStdString(sent.target()), QStringLiteral("c1"));
    QVERIFY(!sent.abort());
    QVERIFY(!sent.continue_());
    QVERIFY(!sent.skip());
    QVERIFY(!sent.no_commit());
    QCOMPARE(QString::fromStdString(sent.message()), QStringLiteral("revert it"));

    fixture.daemon->service().revertConflicts.clear();
    fixture.daemon->service().revertCommitted = true;
    DaemonOperation *continueOperation = fixture.channel->revert(
        "/work/assets", QStringLiteral("c1"), false, true, false, false, 0, QString());
    QSignalSpy continueSpy(continueOperation, &DaemonOperation::finished);
    QTRY_COMPARE(continueSpy.count(), 1);
    const OperationResult continued = continueSpy.last().at(0).value<OperationResult>();
    QVERIFY(continued.revert.committed);
    QVERIFY(fixture.daemon->service().revertRequests[1].continue_());
}

void DaemonChannelTest::loginFlow()
{
    auto fixture = makeConnectedFixture();
    QVERIFY(fixture.channel->isConnected());

    QSignalSpy spy(fixture.channel.get(), &DaemonChannel::loginFinished);
    fixture.channel->login(QStringLiteral("nipa.example.com"), QStringLiteral("ada"),
                           QStringLiteral("secret"));
    QTRY_COMPARE(spy.count(), 1);
    QVERIFY(spy.last().at(0).toBool());
    QCOMPARE(fixture.daemon->service().loginRequests.size(), std::size_t(1));
    QCOMPARE(QString::fromStdString(fixture.daemon->service().loginRequests[0].host()),
             QStringLiteral("nipa.example.com"));
    QCOMPARE(QString::fromStdString(fixture.daemon->service().loginRequests[0].username()),
             QStringLiteral("ada"));
    QCOMPARE(QString::fromStdString(fixture.daemon->service().loginRequests[0].password()),
             QStringLiteral("secret"));

    fixture.daemon->service().loginShouldFail = true;
    spy.clear();
    fixture.channel->login(QStringLiteral("nipa.example.com"), QStringLiteral("ada"),
                           QStringLiteral("wrong"));
    QTRY_COMPARE(spy.count(), 1);
    QVERIFY(!spy.last().at(0).toBool());
    QVERIFY(spy.last().at(1).toString().contains(QStringLiteral("invalid credentials")));
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
