#include "daemonoperation.h"

namespace {

OperationResult resultFromProto(const nipadaemon::OpResult &result)
{
    OperationResult out;
    switch (result.outcome_case()) {
    case nipadaemon::OpResult::kSync:
        out.kind = OperationResult::Sync;
        out.sync.branch = QString::fromStdString(result.sync().branch());
        out.sync.commitId = QString::fromStdString(result.sync().commit_id());
        break;
    case nipadaemon::OpResult::kPush:
        out.kind = OperationResult::Push;
        out.push.commitId = QString::fromStdString(result.push().commit_id());
        out.push.commitHash = QString::fromStdString(result.push().commit_hash());
        out.push.treeHash = QString::fromStdString(result.push().tree_hash());
        break;
    case nipadaemon::OpResult::kMerge:
        out.kind = OperationResult::Merge;
        out.merge.upToDate = result.merge().up_to_date();
        out.merge.fastForwarded = result.merge().fast_forwarded();
        out.merge.mergeCommitted = result.merge().merge_committed();
        out.merge.aborted = result.merge().aborted();
        for (const auto &path : result.merge().conflicts()) {
            out.merge.conflicts.append(QString::fromStdString(path));
        }
        break;
    case nipadaemon::OpResult::kRevert:
        out.kind = OperationResult::Revert;
        out.revert.committed = result.revert().committed();
        out.revert.noChange = result.revert().no_change();
        out.revert.skipped = result.revert().skipped();
        out.revert.aborted = result.revert().aborted();
        for (const auto &path : result.revert().conflicts()) {
            out.revert.conflicts.append(QString::fromStdString(path));
        }
        break;
    case nipadaemon::OpResult::OUTCOME_NOT_SET:
        break;
    }
    return out;
}

} // namespace

DaemonOperation::DaemonOperation(Kind kind, QObject *parent)
    : QObject(parent)
    , kind_(kind)
    , watcher_(new QFutureWatcher<void>(this))
{
    qRegisterMetaType<OpProgressInfo>();
    qRegisterMetaType<OperationResult>();
}

DaemonOperation::~DaemonOperation()
{
    cancel();
    if (watcher_ != nullptr) {
        // The worker exits promptly once the context is cancelled, so this wait
        // is bounded; it guarantees no invokeMethod targets a dying object.
        watcher_->waitForFinished();
    }
}

DaemonOperation::Kind DaemonOperation::kind() const
{
    return kind_;
}

bool DaemonOperation::isRunning() const
{
    return running_;
}

bool DaemonOperation::isCancelled() const
{
    return cancelled_.load();
}

void DaemonOperation::cancel()
{
    if (!running_) {
        return;
    }
    cancelled_.store(true);
    if (context_) {
        context_->TryCancel();
    }
}

void DaemonOperation::setCancelContext(std::shared_ptr<grpc::ClientContext> context)
{
    context_ = std::move(context);
}

void DaemonOperation::setWork(QFuture<void> future)
{
    watcher_->setFuture(future);
}

void DaemonOperation::markTerminal()
{
    running_ = false;
}

void DaemonOperation::handleEvent(const nipadaemon::OpEvent &event)
{
    if (!running_) {
        return;
    }
    switch (event.event_case()) {
    case nipadaemon::OpEvent::kQueued:
        emit queued(event.queued().ahead());
        break;
    case nipadaemon::OpEvent::kStarted:
        emit started(QString::fromStdString(event.started().phase()));
        break;
    case nipadaemon::OpEvent::kProgress: {
        OpProgressInfo info;
        info.phase = QString::fromStdString(event.progress().phase());
        info.objectsDone = event.progress().objects_done();
        info.objectsTotal = event.progress().objects_total();
        info.bytesDone = event.progress().bytes_done();
        info.bytesTotal = event.progress().bytes_total();
        emit progress(info);
        break;
    }
    case nipadaemon::OpEvent::kResult: {
        const OperationResult result = resultFromProto(event.result());
        markTerminal();
        emit finished(result);
        break;
    }
    case nipadaemon::OpEvent::kFailure: {
        const int code = event.failure().code();
        const QString message = QString::fromStdString(event.failure().message());
        markTerminal();
        if (cancelled_.load() || code == 499) {
            emit cancelled();
        } else {
            emit failed(code, message);
        }
        break;
    }
    case nipadaemon::OpEvent::EVENT_NOT_SET:
        break;
    }
}
