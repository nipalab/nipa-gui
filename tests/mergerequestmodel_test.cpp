#include "models/mergerequestmodel.h"

#include <QtTest>

class MergeRequestModelTest : public QObject {
    Q_OBJECT

private slots:
    void emptyModelHasNoRows();
    void displaysMergeRequestColumns();
    void findsRowsByNumber();
    void clearResetsEverything();

private:
    static QList<MergeRequestInfo> sampleMergeRequests();
};

QList<MergeRequestInfo> MergeRequestModelTest::sampleMergeRequests()
{
    MergeRequestInfo open;
    open.id = QStringLiteral("mr-1");
    open.number = 1;
    open.title = QStringLiteral("Add feature");
    open.description = QStringLiteral("Adds the thing");
    open.sourceBranch = QStringLiteral("feature");
    open.targetBranch = QStringLiteral("main");
    open.status = QStringLiteral("open");
    open.createdBy = QStringLiteral("u1");
    open.updatedAt = QDateTime::fromSecsSinceEpoch(1700000000, QTimeZone::UTC);

    MergeRequestInfo merged;
    merged.id = QStringLiteral("mr-2");
    merged.number = 2;
    merged.title = QStringLiteral("Fix bug");
    merged.sourceBranch = QStringLiteral("fix");
    merged.targetBranch = QStringLiteral("main");
    merged.status = QStringLiteral("merged");
    merged.updatedAt = QDateTime::fromSecsSinceEpoch(1700003600, QTimeZone::UTC);

    return {open, merged};
}

void MergeRequestModelTest::emptyModelHasNoRows()
{
    MergeRequestModel model;
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(model.columnCount(), 6);
    QCOMPARE(model.numberAt(0), qint64(0));
    QCOMPARE(model.rowForNumber(1), -1);
}

void MergeRequestModelTest::displaysMergeRequestColumns()
{
    MergeRequestModel model;
    model.setMergeRequests(sampleMergeRequests());

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.index(0, MergeRequestModel::NumberColumn).data().toString(),
             QStringLiteral("#1"));
    QCOMPARE(model.index(0, MergeRequestModel::TitleColumn).data().toString(),
             QStringLiteral("Add feature"));
    QCOMPARE(model.index(0, MergeRequestModel::SourceColumn).data().toString(),
             QStringLiteral("feature"));
    QCOMPARE(model.index(0, MergeRequestModel::TargetColumn).data().toString(),
             QStringLiteral("main"));
    QCOMPARE(model.index(0, MergeRequestModel::StatusColumn).data().toString(),
             QStringLiteral("open"));
    QCOMPARE(model.index(1, MergeRequestModel::StatusColumn).data().toString(),
             QStringLiteral("merged"));

    const QString tip = model.index(0, MergeRequestModel::TitleColumn).data(Qt::ToolTipRole).toString();
    QVERIFY(tip.contains(QStringLiteral("Adds the thing")));
    QVERIFY(tip.contains(QStringLiteral("feature → main")));
}

void MergeRequestModelTest::findsRowsByNumber()
{
    MergeRequestModel model;
    model.setMergeRequests(sampleMergeRequests());

    QCOMPARE(model.rowForNumber(2), 1);
    QCOMPARE(model.numberAt(1), qint64(2));
    const MergeRequestInfo mergeRequest = model.mergeRequestAt(1);
    QCOMPARE(mergeRequest.title, QStringLiteral("Fix bug"));
    QCOMPARE(mergeRequest.status, QStringLiteral("merged"));
}

void MergeRequestModelTest::clearResetsEverything()
{
    MergeRequestModel model;
    model.setMergeRequests(sampleMergeRequests());
    QCOMPARE(model.rowCount(), 2);

    model.clear();
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(model.mergeRequests().size(), 0);
}

QTEST_GUILESS_MAIN(MergeRequestModelTest)

#include "mergerequestmodel_test.moc"
