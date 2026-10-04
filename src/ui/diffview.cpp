#include "diffview.h"

#include <QColor>
#include <QFont>
#include <QFontDatabase>
#include <QTextCursor>

DiffView::DiffView(QWidget *parent)
    : QPlainTextEdit(parent)
{
    setReadOnly(true);
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setUndoRedoEnabled(false);
    setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
}

void DiffView::clearDocument()
{
    clear();
    pending_.clear();
}

void DiffView::appendChunk(const QByteArray &chunk)
{
    pending_ += QString::fromUtf8(chunk);
    int newline = -1;
    while ((newline = pending_.indexOf(QLatin1Char('\n'))) >= 0) {
        const QString line = pending_.left(newline);
        pending_.remove(0, newline + 1);
        appendLine(line);
    }
}

void DiffView::finishDocument()
{
    if (!pending_.isEmpty()) {
        appendLine(pending_);
        pending_.clear();
    }
    if (document()->isEmpty()) {
        appendMessage(tr("(no differences)"));
    }
}

void DiffView::appendMessage(const QString &message)
{
    QTextCursor cursor(document());
    cursor.movePosition(QTextCursor::End);
    QTextCharFormat format;
    format.setForeground(QColor(0x6e, 0x77, 0x81));
    format.setFontItalic(true);
    if (!document()->isEmpty()) {
        cursor.insertBlock();
    }
    cursor.insertText(message, format);
}

void DiffView::appendLine(const QString &line)
{
    QTextCursor cursor(document());
    cursor.movePosition(QTextCursor::End);
    if (!document()->isEmpty()) {
        cursor.insertBlock();
    }
    cursor.insertText(line, formatForLine(line));
}

QTextCharFormat DiffView::formatForLine(const QString &line)
{
    QTextCharFormat format;
    if (line.startsWith(QStringLiteral("@@"))) {
        format.setForeground(QColor(0x09, 0x69, 0xda));
        format.setFontWeight(QFont::Bold);
    } else if (line.startsWith(QStringLiteral("diff "))
               || line.startsWith(QStringLiteral("index "))
               || line.startsWith(QStringLiteral("--- "))
               || line.startsWith(QStringLiteral("+++ "))
               || line.startsWith(QStringLiteral("new file"))
               || line.startsWith(QStringLiteral("deleted file"))
               || line.startsWith(QStringLiteral("rename "))
               || line.startsWith(QStringLiteral("similarity "))
               || line.startsWith(QStringLiteral("Binary files"))) {
        format.setFontWeight(QFont::Bold);
    } else if (line.startsWith(QLatin1Char('+'))) {
        format.setForeground(QColor(0x1a, 0x7f, 0x37));
    } else if (line.startsWith(QLatin1Char('-'))) {
        format.setForeground(QColor(0xcf, 0x22, 0x2e));
    } else if (line.startsWith(QStringLiteral("\\ No newline"))) {
        format.setForeground(QColor(0x6e, 0x77, 0x81));
        format.setFontItalic(true);
    }
    return format;
}
