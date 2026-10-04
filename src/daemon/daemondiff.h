#pragma once

#include <QByteArray>
#include <QFuture>
#include <QFutureWatcher>
#include <QObject>
#include <QString>

#include <atomic>
#include <memory>

#include <grpcpp/grpcpp.h>

#include "daemon.pb.h"

/// One streaming `Diff` RPC. Chunks are plain text; the view colors and pages
/// them. Cancellation closes the gRPC context and ends the stream.
class DaemonDiff : public QObject {
    Q_OBJECT

public:
    bool isRunning() const;
    bool isCancelled() const;

    /// Aborts the stream; cancelled() is emitted once the reader stops.
    void cancel();

    /// Internal: invoked by the transport on this object's thread.
    void handleEvent(const nipadaemon::DiffEvent &event);

    /// Internal: called by the transport when the stream ends cleanly.
    void reportFinished();

signals:
    void dataReceived(const QByteArray &chunk);
    void failed(int code, const QString &message);
    void finished();
    void cancelled();

private:
    friend class DaemonChannel;
    explicit DaemonDiff(QObject *parent = nullptr);
    ~DaemonDiff() override;

    void setCancelContext(std::shared_ptr<grpc::ClientContext> context);
    void setWork(QFuture<void> future);
    void markTerminal();

    std::atomic<bool> cancelled_{false};
    bool running_ = true;
    std::shared_ptr<grpc::ClientContext> context_;
    QFutureWatcher<void> *watcher_ = nullptr;
};
