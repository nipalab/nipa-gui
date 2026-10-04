#pragma once

#include <QDialog>
#include <QString>

class QLineEdit;
class QPushButton;

/// Server login for the daemon's warm transport. The daemon stores the
/// resulting session; the client never persists the password.
class LoginDialog : public QDialog {
    Q_OBJECT

public:
    explicit LoginDialog(const QString &hostPrefill = QString(), QWidget *parent = nullptr);

    QString host() const;
    QString username() const;
    QString password() const;

private:
    QLineEdit *hostEdit_ = nullptr;
    QLineEdit *usernameEdit_ = nullptr;
    QLineEdit *passwordEdit_ = nullptr;
    QPushButton *loginButton_ = nullptr;
};
