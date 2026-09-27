#include "daemonconnection.h"

#include "fakedaemon.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class DaemonConnectionTest : public QObject {
    Q_OBJECT

private slots:
    void missingConfigReportsError();
    void invalidConfigReportsError();
    void incompleteConfigReportsError();
    void unreachableDaemonReportsError();
    void rejectedRequestReportsError();
    void pingReturnsDaemonStatus();
    void defaultConfigPathIsUsed();
};

void DaemonConnectionTest::missingConfigReportsError()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const DaemonConnection connection(tmp.path() + QStringLiteral("/missing/daemon.json"));
    DaemonStatus status;
    QString error;
    QVERIFY(!connection.ping(&status, &error));
    QVERIFY(error.contains(QStringLiteral("cannot read")));
}

void DaemonConnectionTest::invalidConfigReportsError()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = tmp.path() + QStringLiteral("/daemon.json");

    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("{ this is not json");
    file.close();

    const DaemonConnection connection(path);
    DaemonStatus status;
    QString error;
    QVERIFY(!connection.ping(&status, &error));
    QVERIFY(error.contains(QStringLiteral("invalid daemon record")));
}

void DaemonConnectionTest::incompleteConfigReportsError()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString noPort = tmp.path() + QStringLiteral("/no-port/daemon.json");
    QVERIFY(!writeDaemonFile(noPort, 0, QStringLiteral("secret")).isEmpty());
    const DaemonConnection noPortConnection(noPort);
    DaemonStatus status;
    QString error;
    QVERIFY(!noPortConnection.ping(&status, &error));
    QVERIFY(error.contains(QStringLiteral("incomplete")));

    const QString noToken = tmp.path() + QStringLiteral("/no-token/daemon.json");
    QVERIFY(!writeDaemonFile(noToken, 1, QString()).isEmpty());
    const DaemonConnection noTokenConnection(noToken);
    QVERIFY(!noTokenConnection.ping(&status, &error));
    QVERIFY(error.contains(QStringLiteral("incomplete")));
}

void DaemonConnectionTest::unreachableDaemonReportsError()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString path = tmp.path() + QStringLiteral("/daemon.json");
    QVERIFY(!writeDaemonFile(path, 1, QStringLiteral("secret")).isEmpty());

    const DaemonConnection connection(path);
    DaemonStatus status;
    QString error;
    QVERIFY(!connection.ping(&status, &error));
    QVERIFY(error.contains(QStringLiteral("not answering")));
}

void DaemonConnectionTest::rejectedRequestReportsError()
{
    FakeDaemon daemon;
    QVERIFY(daemon.port() > 0);
    daemon.service().requiredToken = "expected";

    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = tmp.path() + QStringLiteral("/daemon.json");
    QVERIFY(!writeDaemonFile(path, daemon.port(), QStringLiteral("wrong")).isEmpty());

    const DaemonConnection connection(path);
    DaemonStatus status;
    QString error;
    QVERIFY(!connection.ping(&status, &error));
    QVERIFY(error.contains(QStringLiteral("rejected the request")));
}

void DaemonConnectionTest::pingReturnsDaemonStatus()
{
    FakeDaemon daemon;
    QVERIFY(daemon.port() > 0);

    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = tmp.path() + QStringLiteral("/daemon.json");
    QVERIFY(!writeDaemonFile(path, daemon.port(), QStringLiteral("sekret")).isEmpty());

    const DaemonConnection connection(path);
    DaemonStatus status;
    QString error;
    QVERIFY(connection.ping(&status, &error));
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(status.version, QStringLiteral("9.9.9"));
    QCOMPARE(status.pid, 4242);
    QCOMPARE(status.endpoint, QStringLiteral("127.0.0.1:") + QString::number(daemon.port()));
    QCOMPARE(QString::fromStdString(daemon.service().token), QStringLiteral("sekret"));
}

void DaemonConnectionTest::defaultConfigPathIsUsed()
{
#ifdef Q_OS_LINUX
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("XDG_CONFIG_HOME", tmp.path().toUtf8());

    const DaemonConnection connection;
    DaemonStatus status;
    QString error;
    QVERIFY(!connection.ping(&status, &error));
    QVERIFY(error.contains(tmp.path() + QStringLiteral("/nipa/daemon.json")));

    qunsetenv("XDG_CONFIG_HOME");
#else
    QSKIP("the default config path is only overridable through XDG_CONFIG_HOME on Linux");
#endif
}

QTEST_GUILESS_MAIN(DaemonConnectionTest)

#include "daemonconnection_test.moc"
