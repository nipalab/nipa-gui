#pragma once

#include <QMainWindow>

class QLabel;
class QPushButton;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void refresh();

private:
    QLabel *statusLabel_ = nullptr;
    QPushButton *refreshButton_ = nullptr;
};
