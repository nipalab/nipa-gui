#include "submitdialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

SubmitDialog::SubmitDialog(const QStringList &stagedFiles, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Submit"));
    resize(560, 420);

    auto *layout = new QVBoxLayout(this);

    layout->addWidget(new QLabel(tr("&Description:"), this));
    messageEdit_ = new QPlainTextEdit(this);
    messageEdit_->setObjectName(QStringLiteral("submitMessageEdit"));
    messageEdit_->setPlaceholderText(tr("Describe this submission"));
    layout->addWidget(messageEdit_, 2);

    layout->addWidget(new QLabel(tr("Staged files (%1):").arg(stagedFiles.size()), this));
    auto *files = new QListWidget(this);
    files->setObjectName(QStringLiteral("submitFilesList"));
    files->setSelectionMode(QAbstractItemView::NoSelection);
    files->addItems(stagedFiles);
    layout->addWidget(files, 3);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    submitButton_ = buttons->button(QDialogButtonBox::Ok);
    submitButton_->setText(tr("Submit"));
    submitButton_->setEnabled(false);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(messageEdit_, &QPlainTextEdit::textChanged, this, [this] {
        submitButton_->setEnabled(!messageEdit_->toPlainText().trimmed().isEmpty());
    });
}

QString SubmitDialog::message() const
{
    return messageEdit_->toPlainText().trimmed();
}
