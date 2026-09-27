#include "mainwindow.h"

#include "fakedaemon.h"

#include <QLabel>
#include <QPushButton>
#include <QTemporaryDir>
#include <QtTest>

class MainWindowTest : public QObject {
    Q_OBJECT

private slots:
    void showsDisconnectedStatus();
    void refreshConnectsAfterDaemonAppears();
};

void MainWindowTest::showsDisconnectedStatus()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    MainWindow window(nullptr, tmp.path() + QStringLiteral("/missing/daemon.json"));
    auto *label = window.findChild<QLabel *>();
    QVERIFY(label != nullptr);
    QVERIFY(label->text().startsWith(QStringLiteral("Not connected:")));
}

void MainWindowTest::refreshConnectsAfterDaemonAppears()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString configPath = tmp.path() + QStringLiteral("/nipa/daemon.json");

    MainWindow window(nullptr, configPath);
    auto *label = window.findChild<QLabel *>();
    auto *button = window.findChild<QPushButton *>();
    QVERIFY(label != nullptr);
    QVERIFY(button != nullptr);
    QVERIFY(label->text().startsWith(QStringLiteral("Not connected:")));

    FakeDaemon daemon;
    QVERIFY(daemon.port() > 0);
    QVERIFY(!writeDaemonFile(configPath, daemon.port(), QStringLiteral("token")).isEmpty());

    button->click();
    QVERIFY(label->text().contains(QStringLiteral("Connected to nipa daemon 9.9.9 (pid 4242) at 127.0.0.1:")));
}

QTEST_MAIN(MainWindowTest)

#include "mainwindow_test.moc"
