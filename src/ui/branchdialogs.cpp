#include "branchdialogs.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

CreateBranchDialog::CreateBranchDialog(const QList<BranchInfo> &branches,
                                       const QString &currentBranch,
                                       QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("New Branch"));
    resize(420, 0);

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout();

    nameEdit_ = new QLineEdit(this);
    nameEdit_->setObjectName(QStringLiteral("branchNameEdit"));
    nameEdit_->setPlaceholderText(tr("feature/my-branch"));
    form->addRow(tr("&Name:"), nameEdit_);

    fromCombo_ = new QComboBox(this);
    fromCombo_->setObjectName(QStringLiteral("branchFromCombo"));
    fromCombo_->addItem(tr("Default branch"), QString());
    for (const BranchInfo &branch : branches) {
        fromCombo_->addItem(branch.name, branch.name);
    }
    const int currentIndex = fromCombo_->findData(currentBranch);
    if (currentIndex >= 0) {
        fromCombo_->setCurrentIndex(currentIndex);
    }
    form->addRow(tr("&Fork from:"), fromCombo_);
    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    createButton_ = buttons->button(QDialogButtonBox::Ok);
    createButton_->setText(tr("Create"));
    createButton_->setEnabled(false);
    layout->addWidget(buttons);

    connect(nameEdit_, &QLineEdit::textChanged, this, [this] {
        createButton_->setEnabled(!nameEdit_->text().trimmed().isEmpty());
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QString CreateBranchDialog::branchName() const
{
    return nameEdit_->text().trimmed();
}

QString CreateBranchDialog::fromBranch() const
{
    return fromCombo_->currentData().toString();
}

MergeDialog::MergeDialog(const QList<BranchInfo> &branches,
                         const QString &currentBranch,
                         QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Merge"));
    resize(440, 0);

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout();

    sourceCombo_ = new QComboBox(this);
    sourceCombo_->setObjectName(QStringLiteral("mergeSourceCombo"));
    for (const BranchInfo &branch : branches) {
        if (branch.name != currentBranch) {
            sourceCombo_->addItem(branch.name, branch.name);
        }
    }
    form->addRow(tr("Merge &from:"), sourceCombo_);

    messageEdit_ = new QLineEdit(this);
    messageEdit_->setObjectName(QStringLiteral("mergeMessageEdit"));
    messageEdit_->setPlaceholderText(tr("Optional merge commit message"));
    form->addRow(tr("&Message:"), messageEdit_);
    layout->addLayout(form);

    layout->addWidget(new QLabel(tr("Current branch: %1").arg(currentBranch), this));

    ffOnlyCheck_ = new QCheckBox(tr("Fast-forward only"), this);
    ffOnlyCheck_->setObjectName(QStringLiteral("mergeFfOnlyCheck"));
    noFfCheck_ = new QCheckBox(tr("Always create a merge commit"), this);
    noFfCheck_->setObjectName(QStringLiteral("mergeNoFfCheck"));
    layout->addWidget(ffOnlyCheck_);
    layout->addWidget(noFfCheck_);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mergeButton_ = buttons->button(QDialogButtonBox::Ok);
    mergeButton_->setText(tr("Merge"));
    mergeButton_->setEnabled(sourceCombo_->count() > 0);
    layout->addWidget(buttons);

    connect(ffOnlyCheck_, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked && noFfCheck_->isChecked()) {
            noFfCheck_->setChecked(false);
        }
    });
    connect(noFfCheck_, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked && ffOnlyCheck_->isChecked()) {
            ffOnlyCheck_->setChecked(false);
        }
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QString MergeDialog::sourceBranch() const
{
    return sourceCombo_->currentData().toString();
}

bool MergeDialog::ffOnly() const
{
    return ffOnlyCheck_->isChecked();
}

bool MergeDialog::noFf() const
{
    return noFfCheck_->isChecked();
}

QString MergeDialog::message() const
{
    return messageEdit_->text().trimmed();
}
