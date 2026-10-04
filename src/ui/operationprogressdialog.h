#pragma once

#include <QDialog>
#include <QString>

#include "daemon/daemontypes.h"

class QLabel;
class QProgressBar;
class QPushButton;

class DaemonOperation;

/// Modal progress for a streamed daemon operation (`Update`, `Push`, ...):
/// phase text, object/byte progress and a Cancel button that aborts the stream.
class OperationProgressDialog : public QDialog {
    Q_OBJECT

public:
    OperationProgressDialog(DaemonOperation *operation, const QString &title, QWidget *parent = nullptr);

    /// Runs modally; returns true when the operation finished successfully.
    bool run();

    bool wasCancelled() const;
    QString errorMessage() const;
    QString resultSummary() const;
    OperationResult result() const;

private:
    void onQueued(int operationsAhead);
    void onStarted(const QString &phase);
    void onProgress(const OpProgressInfo &progress);
    void onFinished(const OperationResult &result);
    void onFailed(int code, const QString &message);
    void onCancelled();

    DaemonOperation *operation_;
    QLabel *phaseLabel_ = nullptr;
    QLabel *detailLabel_ = nullptr;
    QProgressBar *progressBar_ = nullptr;
    QPushButton *cancelButton_ = nullptr;

    bool success_ = false;
    bool cancelled_ = false;
    QString errorMessage_;
    QString resultSummary_;
    OperationResult result_;
};
