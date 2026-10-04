#include "daemon/daemonconnection.h"

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
    void loadsEndpointFromConfig();
    void defaultConfigPathIsUsed();
};

void DaemonConnectionTest::missingConfigReportsError()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    DaemonConnection connection(tmp.path() + QStringLiteral("/missing/daemon.json"));
    DaemonEndpoint endpoint;
    QString error;
    QVERIFY(!connection.load(&endpoint, &error));
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

    DaemonConnection connection(path);
    DaemonEndpoint endpoint;
    QString error;
    QVERIFY(!connection.load(&endpoint, &error));
    QVERIFY(error.contains(QStringLiteral("invalid daemon record")));
}

void DaemonConnectionTest::incompleteConfigReportsError()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString noPort = tmp.path() + QStringLiteral("/no-port/daemon.json");
    QVERIFY(!writeDaemonFile(noPort, 0, QStringLiteral("secret")).isEmpty());
    DaemonConnection noPortConnection(noPort);
    DaemonEndpoint endpoint;
    QString error;
    QVERIFY(!noPortConnection.load(&endpoint, &error));
    QVERIFY(error.contains(QStringLiteral("incomplete")));

    const QString noToken = tmp.path() + QStringLiteral("/no-token/daemon.json");
    QVERIFY(!writeDaemonFile(noToken, 1, QString()).isEmpty());
    DaemonConnection noTokenConnection(noToken);
    QVERIFY(!noTokenConnection.load(&endpoint, &error));
    QVERIFY(error.contains(QStringLiteral("incomplete")));
}

void DaemonConnectionTest::loadsEndpointFromConfig()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = tmp.path() + QStringLiteral("/daemon.json");
    QVERIFY(!writeDaemonFile(path, 1234, QStringLiteral("sekret")).isEmpty());

    DaemonConnection connection(path);
    QCOMPARE(connection.configPath(), path);

    DaemonEndpoint endpoint;
    QString error;
    QVERIFY2(connection.load(&endpoint, &error), qPrintable(error));
    QCOMPARE(endpoint.port, 1234);
    QCOMPARE(endpoint.token, QStringLiteral("sekret"));
    QCOMPARE(endpoint.version, QStringLiteral("9.9.9"));
    QCOMPARE(endpoint.pid, 4242);
    QCOMPARE(endpoint.address(), QStringLiteral("127.0.0.1:1234"));
}

void DaemonConnectionTest::defaultConfigPathIsUsed()
{
#ifdef Q_OS_LINUX
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    qputenv("XDG_CONFIG_HOME", tmp.path().toUtf8());

    DaemonConnection connection;
    QCOMPARE(connection.configPath(), tmp.path() + QStringLiteral("/nipa/daemon.json"));

    DaemonEndpoint endpoint;
    QString error;
    QVERIFY(!connection.load(&endpoint, &error));
    QVERIFY(error.contains(tmp.path() + QStringLiteral("/nipa/daemon.json")));

    qunsetenv("XDG_CONFIG_HOME");
#else
    QSKIP("the default config path is only overridable through XDG_CONFIG_HOME on Linux");
#endif
}

QTEST_GUILESS_MAIN(DaemonConnectionTest)

#include "daemonconnection_test.moc"
