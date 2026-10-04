#include "daemondiff.h"

DaemonDiff::DaemonDiff(QObject *parent)
    : QObject(parent)
    , watcher_(new QFutureWatcher<void>(this))
{
}

DaemonDiff::~DaemonDiff()
{
    cancel();
    if (watcher_ != nullptr) {
        // The reader exits promptly once the context is cancelled, so this
        // wait is bounded; it guarantees no invokeMethod targets a dying object.
        watcher_->waitForFinished();
    }
}

bool DaemonDiff::isRunning() const
{
    return running_;
}

bool DaemonDiff::isCancelled() const
{
    return cancelled_.load();
}

void DaemonDiff::cancel()
{
    if (!running_) {
        return;
    }
    cancelled_.store(true);
    if (context_) {
        context_->TryCancel();
    }
}

void DaemonDiff::setCancelContext(std::shared_ptr<grpc::ClientContext> context)
{
    context_ = std::move(context);
}

void DaemonDiff::setWork(QFuture<void> future)
{
    watcher_->setFuture(future);
}

void DaemonDiff::markTerminal()
{
    running_ = false;
}

void DaemonDiff::reportFinished()
{
    if (!running_) {
        return;
    }
    markTerminal();
    emit finished();
}

void DaemonDiff::handleEvent(const nipadaemon::DiffEvent &event)
{
    if (!running_) {
        return;
    }
    switch (event.event_case()) {
    case nipadaemon::DiffEvent::kData:
        emit dataReceived(QByteArray::fromStdString(event.data()));
        break;
    case nipadaemon::DiffEvent::kFailure: {
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
    case nipadaemon::DiffEvent::EVENT_NOT_SET:
        break;
    }
}
