#include "mrdialogs.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

CreateMergeRequestDialog::CreateMergeRequestDialog(const QList<BranchInfo> &branches,
                                                   const QString &currentBranch,
                                                   QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("New Merge Request"));
    resize(520, 420);

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout();

    titleEdit_ = new QLineEdit(this);
    titleEdit_->setObjectName(QStringLiteral("mrTitleEdit"));
    form->addRow(tr("&Title:"), titleEdit_);

    sourceCombo_ = new QComboBox(this);
    sourceCombo_->setObjectName(QStringLiteral("mrSourceCombo"));
    targetCombo_ = new QComboBox(this);
    targetCombo_->setObjectName(QStringLiteral("mrTargetCombo"));
    for (const BranchInfo &branch : branches) {
        sourceCombo_->addItem(branch.name, branch.name);
        targetCombo_->addItem(branch.name, branch.name);
    }
    const int sourceIndex = sourceCombo_->findData(currentBranch);
    if (sourceIndex >= 0) {
        sourceCombo_->setCurrentIndex(sourceIndex);
    }
    for (const BranchInfo &branch : branches) {
        if (branch.isDefault && branch.name != currentBranch) {
            const int targetIndex = targetCombo_->findData(branch.name);
            if (targetIndex >= 0) {
                targetCombo_->setCurrentIndex(targetIndex);
            }
            break;
        }
    }
    form->addRow(tr("&Source:"), sourceCombo_);
    form->addRow(tr("Tar&get:"), targetCombo_);
    layout->addLayout(form);

    layout->addWidget(new QLabel(tr("&Description:"), this));
    descriptionEdit_ = new QPlainTextEdit(this);
    descriptionEdit_->setObjectName(QStringLiteral("mrDescriptionEdit"));
    layout->addWidget(descriptionEdit_, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    createButton_ = buttons->button(QDialogButtonBox::Ok);
    createButton_->setText(tr("Create"));
    createButton_->setEnabled(false);
    layout->addWidget(buttons);

    const auto updateEnabled = [this] {
        const bool valid = !titleEdit_->text().trimmed().isEmpty()
                           && sourceCombo_->currentData().toString()
                                  != targetCombo_->currentData().toString();
        createButton_->setEnabled(valid);
    };
    connect(titleEdit_, &QLineEdit::textChanged, this, updateEnabled);
    connect(sourceCombo_, &QComboBox::currentIndexChanged, this, updateEnabled);
    connect(targetCombo_, &QComboBox::currentIndexChanged, this, updateEnabled);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QString CreateMergeRequestDialog::title() const
{
    return titleEdit_->text().trimmed();
}

QString CreateMergeRequestDialog::description() const
{
    return descriptionEdit_->toPlainText().trimmed();
}

QString CreateMergeRequestDialog::sourceBranch() const
{
    return sourceCombo_->currentData().toString();
}

QString CreateMergeRequestDialog::targetBranch() const
{
    return targetCombo_->currentData().toString();
}

RevertDialog::RevertDialog(const QString &prefillTarget, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Revert"));
    resize(460, 0);

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout();

    targetEdit_ = new QLineEdit(prefillTarget, this);
    targetEdit_->setObjectName(QStringLiteral("revertTargetEdit"));
    targetEdit_->setPlaceholderText(tr("commit id, hash, or <from>..<to> range"));
    form->addRow(tr("&Target:"), targetEdit_);

    mainlineSpin_ = new QSpinBox(this);
    mainlineSpin_->setObjectName(QStringLiteral("revertMainlineSpin"));
    mainlineSpin_->setRange(0, 9);
    mainlineSpin_->setSpecialValueText(tr("none"));
    mainlineSpin_->setToolTip(tr("Mainline parent (1-based) when reverting a merge commit"));
    form->addRow(tr("&Mainline:"), mainlineSpin_);

    messageEdit_ = new QLineEdit(this);
    messageEdit_->setObjectName(QStringLiteral("revertMessageEdit"));
    messageEdit_->setPlaceholderText(tr("Optional commit message"));
    form->addRow(tr("&Message:"), messageEdit_);

    layout->addLayout(form);

    noCommitCheck_ = new QCheckBox(tr("Do not commit (leave the changes in the working copy)"), this);
    noCommitCheck_->setObjectName(QStringLiteral("revertNoCommitCheck"));
    layout->addWidget(noCommitCheck_);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    revertButton_ = buttons->button(QDialogButtonBox::Ok);
    revertButton_->setText(tr("Revert"));
    revertButton_->setEnabled(!prefillTarget.isEmpty());
    layout->addWidget(buttons);

    connect(targetEdit_, &QLineEdit::textChanged, this, [this] {
        revertButton_->setEnabled(!targetEdit_->text().trimmed().isEmpty());
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QString RevertDialog::target() const
{
    return targetEdit_->text().trimmed();
}

int RevertDialog::mainline() const
{
    return mainlineSpin_->value();
}

bool RevertDialog::noCommit() const
{
    return noCommitCheck_->isChecked();
}

QString RevertDialog::message() const
{
    return messageEdit_->text().trimmed();
}
