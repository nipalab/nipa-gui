#include "mainwindow.h"

#include "daemonconnection.h"

#include <QDebug>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include <utility>

MainWindow::MainWindow(QWidget *parent, QString daemonConfigPath)
    : QMainWindow(parent)
    , daemonConfigPath_(std::move(daemonConfigPath))
{
    setWindowTitle(QStringLiteral("nipa-gui"));
    resize(560, 220);

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);

    statusLabel_ = new QLabel(central);
    statusLabel_->setWordWrap(true);
    statusLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    refreshButton_ = new QPushButton(QStringLiteral("Refresh"), central);
    connect(refreshButton_, &QPushButton::clicked, this, &MainWindow::refresh);

    layout->addWidget(statusLabel_);
    layout->addWidget(refreshButton_);
    layout->addStretch();
    setCentralWidget(central);

    refresh();
}

void MainWindow::refresh()
{
    DaemonStatus status;
    QString error;
    const DaemonConnection connection(daemonConfigPath_);
    if (connection.ping(&status, &error)) {
        statusLabel_->setText(tr("Connected to nipa daemon %1 (pid %2) at %3")
                                  .arg(status.version, QString::number(status.pid), status.endpoint));
        qInfo().noquote() << QStringLiteral("connected to nipa daemon %1 (pid %2) at %3")
                                 .arg(status.version, QString::number(status.pid), status.endpoint);
        return;
    }
    statusLabel_->setText(tr("Not connected: %1").arg(error));
    qWarning().noquote() << QStringLiteral("nipa daemon not reachable: %1").arg(error);
}
