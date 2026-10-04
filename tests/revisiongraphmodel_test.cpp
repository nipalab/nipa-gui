#include "models/revisiongraphmodel.h"

#include <QtTest>

class RevisionGraphModelTest : public QObject {
    Q_OBJECT

private slots:
    void emptyModelHasNoRows();
    void linearHistoryUsesOneLane();
    void mergeAddsSecondLane();
    void displaysCommitColumns();
    void clearResetsEverything();

private:
    static QList<CommitInfo> linearCommits();
    static QList<CommitInfo> mergeCommits();
};

QList<CommitInfo> RevisionGraphModelTest::linearCommits()
{
    CommitInfo newest;
    newest.id = QStringLiteral("c3");
    newest.parent1 = QStringLiteral("c2");
    newest.message = QStringLiteral("third");
    CommitInfo middle;
    middle.id = QStringLiteral("c2");
    middle.parent1 = QStringLiteral("c1");
    middle.message = QStringLiteral("second");
    CommitInfo root;
    root.id = QStringLiteral("c1");
    root.message = QStringLiteral("first");
    return {newest, middle, root};
}

QList<CommitInfo> RevisionGraphModelTest::mergeCommits()
{
    CommitInfo merge;
    merge.id = QStringLiteral("m1");
    merge.parent1 = QStringLiteral("a1");
    merge.parent2 = QStringLiteral("b1");
    merge.message = QStringLiteral("merge feature");
    CommitInfo first;
    first.id = QStringLiteral("a1");
    first.parent1 = QStringLiteral("base");
    first.message = QStringLiteral("on main");
    CommitInfo second;
    second.id = QStringLiteral("b1");
    second.parent1 = QStringLiteral("base");
    second.message = QStringLiteral("on feature");
    CommitInfo base;
    base.id = QStringLiteral("base");
    base.message = QStringLiteral("base");
    return {merge, first, second, base};
}

void RevisionGraphModelTest::emptyModelHasNoRows()
{
    RevisionGraphModel model;
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(model.columnCount(), 4);
    QCOMPARE(model.laneAt(0), -1);
}

void RevisionGraphModelTest::linearHistoryUsesOneLane()
{
    RevisionGraphModel model;
    model.setCommits(linearCommits());

    QCOMPARE(model.rowCount(), 3);
    const QList<RevisionGraphModel::GraphRow> rows =
        RevisionGraphModel::layoutGraph(linearCommits());
    QCOMPARE(rows.size(), 3);
    for (const RevisionGraphModel::GraphRow &row : rows) {
        QCOMPARE(row.lane, 0);
        QCOMPARE(row.art, QString(QChar(0x25CF)));
    }
}

void RevisionGraphModelTest::mergeAddsSecondLane()
{
    const QList<RevisionGraphModel::GraphRow> rows = RevisionGraphModel::layoutGraph(mergeCommits());
    QCOMPARE(rows.size(), 4);

    QCOMPARE(rows.at(0).lane, 0);
    QCOMPARE(rows.at(0).art, QString(QChar(0x25CF)));

    QCOMPARE(rows.at(1).lane, 0);
    QCOMPARE(rows.at(1).art, QString(QChar(0x25CF)) + QChar(0x2502));

    QCOMPARE(rows.at(2).lane, 1);
    QCOMPARE(rows.at(2).art, QString(QChar(0x2502)) + QChar(0x25CF));

    QCOMPARE(rows.at(3).lane, 0);
    QCOMPARE(rows.at(3).art, QString(QChar(0x25CF)));

    RevisionGraphModel model;
    model.setCommits(mergeCommits());
    QCOMPARE(model.laneAt(2), 1);
}

void RevisionGraphModelTest::displaysCommitColumns()
{
    RevisionGraphModel model;
    model.setCommits(mergeCommits());

    QCOMPARE(model.index(0, RevisionGraphModel::CommitColumn).data().toString(),
             QStringLiteral("m1"));
    QCOMPARE(model.index(0, RevisionGraphModel::MessageColumn).data().toString(),
             QStringLiteral("merge feature"));
    QVERIFY(model.index(0, RevisionGraphModel::GraphColumn).data().toString().contains(QChar(0x25CF)));

    const QString tip = model.index(0, RevisionGraphModel::CommitColumn).data(Qt::ToolTipRole).toString();
    QVERIFY(tip.contains(QStringLiteral("a1")));
    QVERIFY(tip.contains(QStringLiteral("b1")));
}

void RevisionGraphModelTest::clearResetsEverything()
{
    RevisionGraphModel model;
    model.setCommits(mergeCommits());
    QCOMPARE(model.rowCount(), 4);

    model.clear();
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(model.commits().size(), 0);
}

QTEST_GUILESS_MAIN(RevisionGraphModelTest)

#include "revisiongraphmodel_test.moc"
