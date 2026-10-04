#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>

class QPlainTextEdit;
class QPushButton;

/// P4V-style Submit dialog: a required description plus the staged file list.
/// The message becomes the pushed commit message.
class SubmitDialog : public QDialog {
    Q_OBJECT

public:
    explicit SubmitDialog(const QStringList &stagedFiles, QWidget *parent = nullptr);

    QString message() const;

private:
    QPlainTextEdit *messageEdit_ = nullptr;
    QPushButton *submitButton_ = nullptr;
};
