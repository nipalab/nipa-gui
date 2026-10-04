#include "models/statusmodel.h"

#include <QtTest>

class StatusModelTest : public QObject {
    Q_OBJECT

private slots:
    void emptyModelHasNoRows();
    void mapsCategoriesAndCounts();
    void categoryRoleAndOrder();
    void filterShowsOnlyCategory();
    void clearResetsEverything();
    void badgesAndLabels();
};

namespace {

StatusSnapshot sampleSnapshot()
{
    StatusSnapshot snapshot;
    snapshot.conflicts = {QStringLiteral("merge.bin")};
    snapshot.missing = {QStringLiteral("gone.dat")};
    snapshot.deleted = {QStringLiteral("old.bin")};
    snapshot.modified = {QStringLiteral("tex/diffuse.png")};
    snapshot.staged = {QStringLiteral("new.png")};
    snapshot.untracked = {QStringLiteral("wip.txt")};
    snapshot.branch = QStringLiteral("main");
    return snapshot;
}

} // namespace

void StatusModelTest::emptyModelHasNoRows()
{
    StatusModel model;
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(model.columnCount(), 2);
    for (int category = 0; category < StatusModel::CategoryCount; ++category) {
        QCOMPARE(model.count(static_cast<StatusModel::Category>(category)), 0);
    }
}

void StatusModelTest::mapsCategoriesAndCounts()
{
    StatusModel model;
    model.setSnapshot(sampleSnapshot());

    QCOMPARE(model.rowCount(), 6);
    QCOMPARE(model.count(StatusModel::Staged), 1);
    QCOMPARE(model.count(StatusModel::Deleted), 1);
    QCOMPARE(model.count(StatusModel::Modified), 1);
    QCOMPARE(model.count(StatusModel::Untracked), 1);
    QCOMPARE(model.count(StatusModel::Missing), 1);
    QCOMPARE(model.count(StatusModel::Conflicts), 1);
    QCOMPARE(model.snapshot().branch, QStringLiteral("main"));
}

void StatusModelTest::categoryRoleAndOrder()
{
    StatusModel model;
    model.setSnapshot(sampleSnapshot());

    // Conflicts first, then Missing, Deleted, Modified, Staged, Untracked.
    QCOMPARE(model.index(0, StatusModel::StatusColumn).data().toString(), QStringLiteral("C"));
    QCOMPARE(model.index(0, StatusModel::PathColumn).data().toString(), QStringLiteral("merge.bin"));
    QCOMPARE(model.index(1, StatusModel::StatusColumn).data().toString(), QStringLiteral("!"));
    QCOMPARE(model.index(2, StatusModel::PathColumn).data().toString(), QStringLiteral("old.bin"));
    QCOMPARE(model.index(3, StatusModel::StatusColumn).data().toString(), QStringLiteral("M"));
    QCOMPARE(model.index(4, StatusModel::StatusColumn).data().toString(), QStringLiteral("A"));
    QCOMPARE(model.index(5, StatusModel::StatusColumn).data().toString(), QStringLiteral("?"));

    QCOMPARE(model.index(0, StatusModel::PathColumn).data(StatusModel::CategoryRole).toInt(),
             static_cast<int>(StatusModel::Conflicts));
    QCOMPARE(model.index(4, StatusModel::PathColumn).data(StatusModel::CategoryRole).toInt(),
             static_cast<int>(StatusModel::Staged));
}

void StatusModelTest::filterShowsOnlyCategory()
{
    StatusModel model;
    model.setSnapshot(sampleSnapshot());

    model.setCategoryFilter(StatusModel::Modified);
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.index(0, StatusModel::PathColumn).data().toString(),
             QStringLiteral("tex/diffuse.png"));
    // Counts keep reflecting the whole snapshot.
    QCOMPARE(model.count(StatusModel::Staged), 1);

    model.setCategoryFilter(-1);
    QCOMPARE(model.rowCount(), 6);
}

void StatusModelTest::clearResetsEverything()
{
    StatusModel model;
    model.setSnapshot(sampleSnapshot());
    QCOMPARE(model.rowCount(), 6);

    model.clear();
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(model.snapshot().total(), 0);
    QCOMPARE(model.count(StatusModel::Conflicts), 0);
}

void StatusModelTest::badgesAndLabels()
{
    QCOMPARE(StatusModel::categoryBadge(StatusModel::Staged), QStringLiteral("A"));
    QCOMPARE(StatusModel::categoryBadge(StatusModel::Deleted), QStringLiteral("D"));
    QCOMPARE(StatusModel::categoryBadge(StatusModel::Modified), QStringLiteral("M"));
    QCOMPARE(StatusModel::categoryBadge(StatusModel::Untracked), QStringLiteral("?"));
    QCOMPARE(StatusModel::categoryBadge(StatusModel::Missing), QStringLiteral("!"));
    QCOMPARE(StatusModel::categoryBadge(StatusModel::Conflicts), QStringLiteral("C"));

    QCOMPARE(StatusModel::categoryLabel(StatusModel::Staged), QStringLiteral("Staged"));
    QCOMPARE(StatusModel::categoryLabel(StatusModel::Conflicts), QStringLiteral("Conflicts"));
}

QTEST_GUILESS_MAIN(StatusModelTest)

#include "statusmodel_test.moc"
