#include "logindialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

LoginDialog::LoginDialog(const QString &hostPrefill, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Login"));
    resize(400, 0);

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout();

    hostEdit_ = new QLineEdit(hostPrefill, this);
    hostEdit_->setObjectName(QStringLiteral("loginHostEdit"));
    hostEdit_->setPlaceholderText(tr("nipa.example.com"));
    form->addRow(tr("&Host:"), hostEdit_);

    usernameEdit_ = new QLineEdit(this);
    usernameEdit_->setObjectName(QStringLiteral("loginUsernameEdit"));
    form->addRow(tr("&Username:"), usernameEdit_);

    passwordEdit_ = new QLineEdit(this);
    passwordEdit_->setObjectName(QStringLiteral("loginPasswordEdit"));
    passwordEdit_->setEchoMode(QLineEdit::Password);
    form->addRow(tr("&Password:"), passwordEdit_);

    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    loginButton_ = buttons->button(QDialogButtonBox::Ok);
    loginButton_->setText(tr("Login"));
    loginButton_->setEnabled(false);
    layout->addWidget(buttons);

    const auto updateEnabled = [this] {
        loginButton_->setEnabled(!hostEdit_->text().trimmed().isEmpty()
                                 && !usernameEdit_->text().trimmed().isEmpty());
    };
    connect(hostEdit_, &QLineEdit::textChanged, this, updateEnabled);
    connect(usernameEdit_, &QLineEdit::textChanged, this, updateEnabled);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    updateEnabled();
}

QString LoginDialog::host() const
{
    return hostEdit_->text().trimmed();
}

QString LoginDialog::username() const
{
    return usernameEdit_->text().trimmed();
}

QString LoginDialog::password() const
{
    return passwordEdit_->text();
}
