#include "models/locksmodel.h"

#include <QtTest>

class LocksModelTest : public QObject {
    Q_OBJECT

private slots:
    void emptyModelHasNoRows();
    void displaysScopeAndHolder();
    void exposesLockAtAndPathAt();
    void clearResetsEverything();

private:
    static QList<FileLockInfo> sampleLocks();
};

QList<FileLockInfo> LocksModelTest::sampleLocks()
{
    FileLockInfo mainline;
    mainline.id = QStringLiteral("l1");
    mainline.path = QStringLiteral("assets/logo.png");
    mainline.global = true;
    mainline.heldByName = QStringLiteral("Maya");
    mainline.acquiredAt = QDateTime::fromSecsSinceEpoch(1700000000, QTimeZone::UTC);

    FileLockInfo branch;
    branch.id = QStringLiteral("l2");
    branch.path = QStringLiteral("assets/textures");
    branch.branch = QStringLiteral("feature/x");
    branch.heldBy = QStringLiteral("user-9");
    branch.hasMergeRequest = true;
    branch.mergeRequestNumber = 42;
    branch.acquiredAt = QDateTime::fromSecsSinceEpoch(1700003600, QTimeZone::UTC);

    return {mainline, branch};
}

void LocksModelTest::emptyModelHasNoRows()
{
    LocksModel model;
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(model.columnCount(), 4);
    QCOMPARE(model.pathAt(0), QString());
}

void LocksModelTest::displaysScopeAndHolder()
{
    LocksModel model;
    model.setLocks(sampleLocks());

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.index(0, LocksModel::PathColumn).data().toString(),
             QStringLiteral("assets/logo.png"));
    QCOMPARE(model.index(0, LocksModel::ScopeColumn).data().toString(), QStringLiteral("Mainline"));
    QCOMPARE(model.index(0, LocksModel::HolderColumn).data().toString(), QStringLiteral("Maya"));
    QCOMPARE(model.index(1, LocksModel::ScopeColumn).data().toString(), QStringLiteral("feature/x"));
    // Falls back to the user id when no display name is known.
    QCOMPARE(model.index(1, LocksModel::HolderColumn).data().toString(), QStringLiteral("user-9"));

    const QString tip = model.index(1, LocksModel::PathColumn).data(Qt::ToolTipRole).toString();
    QVERIFY(tip.contains(QStringLiteral("Merge request #42")));

    QCOMPARE(LocksModel::scopeLabel(sampleLocks().first()), QStringLiteral("Mainline"));
    QCOMPARE(LocksModel::scopeLabel(sampleLocks().at(1)), QStringLiteral("feature/x"));
}

void LocksModelTest::exposesLockAtAndPathAt()
{
    LocksModel model;
    model.setLocks(sampleLocks());

    QCOMPARE(model.pathAt(1), QStringLiteral("assets/textures"));
    const FileLockInfo lock = model.lockAt(1);
    QCOMPARE(lock.id, QStringLiteral("l2"));
    QVERIFY(lock.hasMergeRequest);
    QCOMPARE(lock.mergeRequestNumber, 42);
}

void LocksModelTest::clearResetsEverything()
{
    LocksModel model;
    model.setLocks(sampleLocks());
    QCOMPARE(model.rowCount(), 2);

    model.clear();
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(model.locks().size(), 0);
}

QTEST_GUILESS_MAIN(LocksModelTest)

#include "locksmodel_test.moc"
