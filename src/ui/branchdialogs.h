#pragma once

#include <QDialog>
#include <QList>
#include <QString>

#include "daemon/daemontypes.h"

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;

/// New-branch dialog: a required name and the branch (or default) to fork from.
class CreateBranchDialog : public QDialog {
    Q_OBJECT

public:
    CreateBranchDialog(const QList<BranchInfo> &branches,
                       const QString &currentBranch,
                       QWidget *parent = nullptr);

    QString branchName() const;
    QString fromBranch() const;

private:
    QLineEdit *nameEdit_ = nullptr;
    QComboBox *fromCombo_ = nullptr;
    QPushButton *createButton_ = nullptr;
};

/// Merge dialog: source branch, fast-forward policy and an optional message.
class MergeDialog : public QDialog {
    Q_OBJECT

public:
    MergeDialog(const QList<BranchInfo> &branches,
                const QString &currentBranch,
                QWidget *parent = nullptr);

    QString sourceBranch() const;
    bool ffOnly() const;
    bool noFf() const;
    QString message() const;

private:
    QComboBox *sourceCombo_ = nullptr;
    QCheckBox *ffOnlyCheck_ = nullptr;
    QCheckBox *noFfCheck_ = nullptr;
    QLineEdit *messageEdit_ = nullptr;
    QPushButton *mergeButton_ = nullptr;
};
