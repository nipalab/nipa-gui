#pragma once

#include <QDialog>
#include <QList>
#include <QString>

#include "daemon/daemontypes.h"

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;

/// New merge request: title, description, source and target branches.
class CreateMergeRequestDialog : public QDialog {
    Q_OBJECT

public:
    CreateMergeRequestDialog(const QList<BranchInfo> &branches,
                             const QString &currentBranch,
                             QWidget *parent = nullptr);

    QString title() const;
    QString description() const;
    QString sourceBranch() const;
    QString targetBranch() const;

private:
    QLineEdit *titleEdit_ = nullptr;
    QPlainTextEdit *descriptionEdit_ = nullptr;
    QComboBox *sourceCombo_ = nullptr;
    QComboBox *targetCombo_ = nullptr;
    QPushButton *createButton_ = nullptr;
};

/// Revert a commit or range, with the mainline/no-commit controls the daemon
/// accepts. Continue/skip/abort are driven by the conflict flow afterwards.
class RevertDialog : public QDialog {
    Q_OBJECT

public:
    explicit RevertDialog(const QString &prefillTarget, QWidget *parent = nullptr);

    QString target() const;
    int mainline() const;
    bool noCommit() const;
    QString message() const;

private:
    QLineEdit *targetEdit_ = nullptr;
    QSpinBox *mainlineSpin_ = nullptr;
    QCheckBox *noCommitCheck_ = nullptr;
    QLineEdit *messageEdit_ = nullptr;
    QPushButton *revertButton_ = nullptr;
};
