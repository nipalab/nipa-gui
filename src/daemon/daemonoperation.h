#pragma once

#include <QFuture>
#include <QFutureWatcher>
#include <QObject>
#include <QString>

#include <atomic>
#include <memory>

#include <grpcpp/grpcpp.h>

#include "daemon.pb.h"
#include "daemontypes.h"

/// One long-running daemon operation streamed as `OpEvent`s (Update, Push,
/// Switch, Merge, Revert). DaemonChannel creates it; the UI observes progress
/// and may cancel. Events are translated on the operation's own thread, so
/// signals are safe to connect to widgets.
class DaemonOperation : public QObject {
    Q_OBJECT

public:
    enum class Kind {
        Update,
        Switch,
        Push,
        Merge,
        Revert,
    };
    Q_ENUM(Kind)

    Kind kind() const;
    bool isRunning() const;
    bool isCancelled() const;

    /// Requests cancellation: the gRPC context is cancelled, the daemon stops
    /// the operation, and the stream ends with cancelled().
    void cancel();

    /// Internal: invoked by the transport on this object's thread.
    void handleEvent(const nipadaemon::OpEvent &event);

signals:
    void queued(int operationsAhead);
    void started(const QString &phase);
    void progress(const OpProgressInfo &progress);
    void finished(const OperationResult &result);
    void failed(int code, const QString &message);
    void cancelled();

private:
    friend class DaemonChannel;
    explicit DaemonOperation(Kind kind, QObject *parent = nullptr);
    ~DaemonOperation() override;

    void setCancelContext(std::shared_ptr<grpc::ClientContext> context);
    void setWork(QFuture<void> future);
    void markTerminal();

    Kind kind_;
    std::atomic<bool> cancelled_{false};
    bool running_ = true;
    std::shared_ptr<grpc::ClientContext> context_;
    QFutureWatcher<void> *watcher_ = nullptr;
};
