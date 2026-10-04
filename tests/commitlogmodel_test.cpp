#include "models/commitlogmodel.h"

#include <QtTest>

class CommitLogModelTest : public QObject {
    Q_OBJECT

private slots:
    void emptyModelHasNoRows();
    void displaysCommitColumns();
    void appendAndFindRows();
    void clearResetsEverything();

private:
    static QList<CommitInfo> sampleCommits();
};

QList<CommitInfo> CommitLogModelTest::sampleCommits()
{
    CommitInfo first;
    first.id = QStringLiteral("abc123def456");
    first.hash = QStringLiteral("deadbeef");
    first.authorName = QStringLiteral("Ada");
    first.authorEmail = QStringLiteral("ada@example.com");
    first.message = QStringLiteral("Initial commit\n\nBody line");
    first.createdAt = QDateTime::fromSecsSinceEpoch(1700000000, QTimeZone::UTC);

    CommitInfo second;
    second.id = QStringLiteral("def456abc123");
    second.hash = QStringLiteral("cafebabe");
    second.parent1 = first.id;
    second.authorName = QStringLiteral("Bob");
    second.authorEmail = QStringLiteral("bob@example.com");
    second.message = QStringLiteral("Second commit");
    second.createdAt = QDateTime::fromSecsSinceEpoch(1700003600, QTimeZone::UTC);

    return {first, second};
}

void CommitLogModelTest::emptyModelHasNoRows()
{
    CommitLogModel model;
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(model.columnCount(), 4);
    QCOMPARE(model.commitIdAt(0), QString());
    QCOMPARE(model.rowForCommitId(QStringLiteral("nope")), -1);
}

void CommitLogModelTest::displaysCommitColumns()
{
    CommitLogModel model;
    model.setCommits(sampleCommits());

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.index(0, CommitLogModel::CommitColumn).data().toString(),
             QStringLiteral("abc123de"));
    QCOMPARE(model.index(0, CommitLogModel::AuthorColumn).data().toString(), QStringLiteral("Ada"));
    QCOMPARE(model.index(0, CommitLogModel::MessageColumn).data().toString(),
             QStringLiteral("Initial commit"));
    QCOMPARE(model.index(1, CommitLogModel::MessageColumn).data().toString(),
             QStringLiteral("Second commit"));

    const QString date = model.index(0, CommitLogModel::DateColumn).data().toString();
    QCOMPARE(date.size(), 16);

    const QString tip = model.index(0, CommitLogModel::AuthorColumn).data(Qt::ToolTipRole).toString();
    QVERIFY(tip.contains(QStringLiteral("abc123def456")));
    QVERIFY(tip.contains(QStringLiteral("deadbeef")));
}

void CommitLogModelTest::appendAndFindRows()
{
    CommitLogModel model;
    const QList<CommitInfo> commits = sampleCommits();
    model.setCommits({commits.first()});
    QCOMPARE(model.rowCount(), 1);

    model.appendCommits({commits.at(1)});
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.commitIdAt(1), QStringLiteral("def456abc123"));
    QCOMPARE(model.rowForCommitId(QStringLiteral("def456abc123")), 1);
    QCOMPARE(model.commits().size(), 2);
}

void CommitLogModelTest::clearResetsEverything()
{
    CommitLogModel model;
    model.setCommits(sampleCommits());
    QCOMPARE(model.rowCount(), 2);

    model.clear();
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(model.commits().size(), 0);
}

QTEST_GUILESS_MAIN(CommitLogModelTest)

#include "commitlogmodel_test.moc"
