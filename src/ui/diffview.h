#pragma once

#include <QByteArray>
#include <QPlainTextEdit>
#include <QString>
#include <QTextCharFormat>

/// Plain-text diff renderer: appends streamed patch chunks with per-line
/// coloring (hunks, additions, deletions, file headers). Rendering is
/// incremental, so large diffs stream in without re-parsing.
class DiffView : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit DiffView(QWidget *parent = nullptr);

    void clearDocument();
    void appendChunk(const QByteArray &chunk);
    void finishDocument();
    void appendMessage(const QString &message);

private:
    void appendLine(const QString &line);
    static QTextCharFormat formatForLine(const QString &line);

    QString pending_;
};
