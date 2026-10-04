#include "operationprogressdialog.h"

#include "daemon/daemonoperation.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

QString formatBytes(qint64 bytes)
{
    constexpr qint64 kKiB = 1024;
    constexpr qint64 kMiB = kKiB * 1024;
    constexpr qint64 kGiB = kMiB * 1024;
    if (bytes < kKiB) {
        return QObject::tr("%1 B").arg(bytes);
    }
    if (bytes < kMiB) {
        return QObject::tr("%1 KiB").arg(bytes / static_cast<double>(kKiB), 0, 'f', 1);
    }
    if (bytes < kGiB) {
        return QObject::tr("%1 MiB").arg(bytes / static_cast<double>(kMiB), 0, 'f', 1);
    }
    return QObject::tr("%1 GiB").arg(bytes / static_cast<double>(kGiB), 0, 'f', 1);
}

} // namespace

OperationProgressDialog::OperationProgressDialog(DaemonOperation *operation,
                                                 const QString &title,
                                                 QWidget *parent)
    : QDialog(parent)
    , operation_(operation)
{
    setWindowTitle(title);
    setModal(true);
    setMinimumWidth(420);

    auto *layout = new QVBoxLayout(this);

    phaseLabel_ = new QLabel(tr("Preparing…"), this);
    phaseLabel_->setObjectName(QStringLiteral("operationPhaseLabel"));
    layout->addWidget(phaseLabel_);

    progressBar_ = new QProgressBar(this);
    progressBar_->setObjectName(QStringLiteral("operationProgressBar"));
    progressBar_->setRange(0, 0);
    layout->addWidget(progressBar_);

    detailLabel_ = new QLabel(this);
    detailLabel_->setObjectName(QStringLiteral("operationDetailLabel"));
    layout->addWidget(detailLabel_);

    auto *buttons = new QDialogButtonBox(this);
    cancelButton_ = buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
    cancelButton_->setObjectName(QStringLiteral("operationCancelButton"));
    layout->addWidget(buttons);

    connect(cancelButton_, &QPushButton::clicked, this, [this] {
        cancelButton_->setEnabled(false);
        phaseLabel_->setText(tr("Cancelling…"));
        operation_->cancel();
    });

    connect(operation_, &DaemonOperation::queued, this, &OperationProgressDialog::onQueued);
    connect(operation_, &DaemonOperation::started, this, &OperationProgressDialog::onStarted);
    connect(operation_, &DaemonOperation::progress, this, &OperationProgressDialog::onProgress);
    connect(operation_, &DaemonOperation::finished, this, &OperationProgressDialog::onFinished);
    connect(operation_, &DaemonOperation::failed, this, &OperationProgressDialog::onFailed);
    connect(operation_, &DaemonOperation::cancelled, this, &OperationProgressDialog::onCancelled);
}

bool OperationProgressDialog::run()
{
    exec();
    return success_;
}

bool OperationProgressDialog::wasCancelled() const
{
    return cancelled_;
}

QString OperationProgressDialog::errorMessage() const
{
    return errorMessage_;
}

QString OperationProgressDialog::resultSummary() const
{
    return resultSummary_;
}

void OperationProgressDialog::onQueued(int operationsAhead)
{
    phaseLabel_->setText(tr("Queued (%n operation(s) ahead)", "", operationsAhead));
    progressBar_->setRange(0, 0);
}

void OperationProgressDialog::onStarted(const QString &phase)
{
    phaseLabel_->setText(phase.isEmpty() ? tr("Working…") : tr("%1…").arg(phase));
    progressBar_->setRange(0, 0);
}

void OperationProgressDialog::onProgress(const OpProgressInfo &progress)
{
    if (progress.objectsTotal > 0) {
        progressBar_->setRange(0, static_cast<int>(progress.objectsTotal));
        progressBar_->setValue(static_cast<int>(progress.objectsDone));
    } else if (progress.bytesTotal > 0) {
        progressBar_->setRange(0, 1000);
        progressBar_->setValue(static_cast<int>(progress.bytesDone * 1000 / progress.bytesTotal));
    } else {
        progressBar_->setRange(0, 0);
    }

    QStringList details;
    if (progress.objectsTotal > 0) {
        details.append(tr("%1 / %2 objects").arg(progress.objectsDone).arg(progress.objectsTotal));
    }
    if (progress.bytesTotal > 0) {
        details.append(tr("%1 / %2").arg(formatBytes(progress.bytesDone), formatBytes(progress.bytesTotal)));
    }
    detailLabel_->setText(details.join(QStringLiteral(" · ")));
}

void OperationProgressDialog::onFinished(const OperationResult &result)
{
    success_ = true;
    switch (result.kind) {
    case OperationResult::Sync:
        resultSummary_ = tr("Updated %1 to %2").arg(result.sync.branch, result.sync.commitId);
        break;
    case OperationResult::Push:
        resultSummary_ = tr("Submitted %1").arg(result.push.commitId);
        break;
    case OperationResult::Merge:
        resultSummary_ = tr("Merge completed");
        break;
    case OperationResult::Revert:
        resultSummary_ = tr("Revert completed");
        break;
    }
    accept();
}

void OperationProgressDialog::onFailed(int code, const QString &message)
{
    errorMessage_ = code > 0 ? tr("%1 (code %2)").arg(message).arg(code) : message;
    reject();
}

void OperationProgressDialog::onCancelled()
{
    cancelled_ = true;
    reject();
}
