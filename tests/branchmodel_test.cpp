#include "models/branchmodel.h"

#include <QtTest>

class BranchModelTest : public QObject {
    Q_OBJECT

private slots:
    void emptyModelHasNoRows();
    void displaysBranches();
    void findsRowsByName();
    void clearResetsEverything();

private:
    static QList<BranchInfo> sampleBranches();
};

QList<BranchInfo> BranchModelTest::sampleBranches()
{
    BranchInfo main;
    main.id = QStringLiteral("b1");
    main.name = QStringLiteral("main");
    main.commitId = QStringLiteral("commit0001");
    main.isDefault = true;
    main.createdAt = QDateTime::fromSecsSinceEpoch(1700000000, QTimeZone::UTC);

    BranchInfo feature;
    feature.id = QStringLiteral("b2");
    feature.name = QStringLiteral("feature/x");
    feature.commitId = QStringLiteral("commit0002");
    feature.isProtected = true;
    feature.createdAt = QDateTime::fromSecsSinceEpoch(1700003600, QTimeZone::UTC);

    BranchInfo empty;
    empty.id = QStringLiteral("b3");
    empty.name = QStringLiteral("empty");

    return {main, feature, empty};
}

void BranchModelTest::emptyModelHasNoRows()
{
    BranchModel model;
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(model.columnCount(), 4);
    QCOMPARE(model.nameAt(0), QString());
    QCOMPARE(model.rowForName(QStringLiteral("nope")), -1);
}

void BranchModelTest::displaysBranches()
{
    BranchModel model;
    model.setBranches(sampleBranches());

    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(model.index(0, BranchModel::NameColumn).data().toString(), QStringLiteral("main"));
    QCOMPARE(model.index(0, BranchModel::HeadColumn).data().toString(), QStringLiteral("commit00"));
    QCOMPARE(model.index(0, BranchModel::DefaultColumn).data().toString(), QStringLiteral("✓"));
    QCOMPARE(model.index(0, BranchModel::ProtectedColumn).data().toString(), QString());

    QCOMPARE(model.index(1, BranchModel::DefaultColumn).data().toString(), QString());
    QCOMPARE(model.index(1, BranchModel::ProtectedColumn).data().toString(), QStringLiteral("✓"));
    QCOMPARE(model.index(2, BranchModel::HeadColumn).data().toString(), QString());

    QVERIFY(model.index(0, BranchModel::NameColumn).data(Qt::ToolTipRole).toString().contains(
        QStringLiteral("commit0001")));
}

void BranchModelTest::findsRowsByName()
{
    BranchModel model;
    model.setBranches(sampleBranches());

    QCOMPARE(model.rowForName(QStringLiteral("feature/x")), 1);
    QCOMPARE(model.nameAt(1), QStringLiteral("feature/x"));
    const BranchInfo branch = model.branchAt(1);
    QVERIFY(branch.isProtected);
    QCOMPARE(branch.commitId, QStringLiteral("commit0002"));
}

void BranchModelTest::clearResetsEverything()
{
    BranchModel model;
    model.setBranches(sampleBranches());
    QCOMPARE(model.rowCount(), 3);

    model.clear();
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(model.branches().size(), 0);
}

QTEST_GUILESS_MAIN(BranchModelTest)

#include "branchmodel_test.moc"
