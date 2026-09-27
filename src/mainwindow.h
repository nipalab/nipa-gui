#pragma once

#include <QMainWindow>
#include <QString>

class QLabel;
class QPushButton;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr, QString daemonConfigPath = QString());

private slots:
    void refresh();

private:
    QLabel *statusLabel_ = nullptr;
    QPushButton *refreshButton_ = nullptr;
    QString daemonConfigPath_;
};
