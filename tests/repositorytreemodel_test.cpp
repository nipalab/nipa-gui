#include "models/repositorytreemodel.h"

#include <QtTest>

class RepositoryTreeModelTest : public QObject {
    Q_OBJECT

private slots:
    void emptyModelHasNoRows();
    void sortsDirectoriesFirstThenNames();
    void exposesNestedPaths();
    void pathAndDirectoryRoles();
    void formatsSizes();
    void clearResetsEverything();

private:
    static TreeNodeData sampleTree();
};

TreeNodeData RepositoryTreeModelTest::sampleTree()
{
    TreeNodeData root;
    root.isDirectory = true;
    root.name = QStringLiteral("root");

    TreeNodeData assets;
    assets.isDirectory = true;
    assets.name = QStringLiteral("assets");
    assets.path = QStringLiteral("assets");

    TreeNodeData logo;
    logo.name = QStringLiteral("logo.png");
    logo.path = QStringLiteral("assets/logo.png");
    logo.isBinary = true;
    logo.sizeBytes = 2048;

    TreeNodeData shaders;
    shaders.isDirectory = true;
    shaders.name = QStringLiteral("shaders");
    shaders.path = QStringLiteral("assets/shaders");

    TreeNodeData water;
    water.name = QStringLiteral("water.glsl");
    water.path = QStringLiteral("assets/shaders/water.glsl");
    water.sizeBytes = 512;

    shaders.children.append(water);
    assets.children.append(logo);
    assets.children.append(shaders);

    TreeNodeData readme;
    readme.name = QStringLiteral("README.md");
    readme.path = QStringLiteral("README.md");
    readme.sizeBytes = 100;

    TreeNodeData zeta;
    zeta.name = QStringLiteral("zeta.bin");
    zeta.path = QStringLiteral("zeta.bin");
    zeta.sizeBytes = 10;

    root.children.append(zeta);
    root.children.append(assets);
    root.children.append(readme);
    return root;
}

void RepositoryTreeModelTest::emptyModelHasNoRows()
{
    RepositoryTreeModel model;
    QVERIFY(model.isEmpty());
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(model.columnCount(), 2);
}

void RepositoryTreeModelTest::sortsDirectoriesFirstThenNames()
{
    RepositoryTreeModel model;
    model.setTree(sampleTree());

    QVERIFY(!model.isEmpty());
    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(model.index(0, RepositoryTreeModel::NameColumn).data().toString(), QStringLiteral("assets"));
    QCOMPARE(model.index(1, RepositoryTreeModel::NameColumn).data().toString(), QStringLiteral("README.md"));
    QCOMPARE(model.index(2, RepositoryTreeModel::NameColumn).data().toString(), QStringLiteral("zeta.bin"));
}

void RepositoryTreeModelTest::exposesNestedPaths()
{
    RepositoryTreeModel model;
    model.setTree(sampleTree());

    const QModelIndex assets = model.index(0, RepositoryTreeModel::NameColumn);
    QCOMPARE(model.rowCount(assets), 2);
    // Directories first inside assets: shaders, then logo.png.
    const QModelIndex shaders = model.index(0, RepositoryTreeModel::NameColumn, assets);
    const QModelIndex logo = model.index(1, RepositoryTreeModel::NameColumn, assets);
    QCOMPARE(shaders.data().toString(), QStringLiteral("shaders"));
    QCOMPARE(logo.data().toString(), QStringLiteral("logo.png"));

    QCOMPARE(model.rowCount(shaders), 1);
    const QModelIndex water = model.index(0, RepositoryTreeModel::NameColumn, shaders);
    QCOMPARE(model.pathForIndex(water), QStringLiteral("assets/shaders/water.glsl"));

    QCOMPARE(model.parent(water), shaders);
    QCOMPARE(model.parent(shaders), assets);
    QVERIFY(!model.parent(assets).isValid());
}

void RepositoryTreeModelTest::pathAndDirectoryRoles()
{
    RepositoryTreeModel model;
    model.setTree(sampleTree());

    const QModelIndex assets = model.index(0, RepositoryTreeModel::NameColumn);
    const QModelIndex readme = model.index(1, RepositoryTreeModel::NameColumn);

    QCOMPARE(assets.data(RepositoryTreeModel::PathRole).toString(), QStringLiteral("assets"));
    QVERIFY(assets.data(RepositoryTreeModel::IsDirectoryRole).toBool());
    QVERIFY(!readme.data(RepositoryTreeModel::IsDirectoryRole).toBool());
    QCOMPARE(model.pathForIndex(readme), QStringLiteral("README.md"));
    QVERIFY(model.isDirectory(assets));
    QVERIFY(!model.isDirectory(readme));
}

void RepositoryTreeModelTest::formatsSizes()
{
    RepositoryTreeModel model;
    model.setTree(sampleTree());

    const QModelIndex assets = model.index(0, RepositoryTreeModel::NameColumn);
    const QModelIndex logo = model.index(1, RepositoryTreeModel::NameColumn, assets);
    const QModelIndex readme = model.index(1, RepositoryTreeModel::NameColumn);

    QCOMPARE(model.index(assets.row(), RepositoryTreeModel::SizeColumn).data().toString(), QString());
    QCOMPARE(model.index(logo.row(), RepositoryTreeModel::SizeColumn, assets).data().toString(),
             QStringLiteral("2.0 KiB"));
    QCOMPARE(model.index(readme.row(), RepositoryTreeModel::SizeColumn).data().toString(),
             QStringLiteral("100 B"));
}

void RepositoryTreeModelTest::clearResetsEverything()
{
    RepositoryTreeModel model;
    model.setTree(sampleTree());
    QCOMPARE(model.rowCount(), 3);

    model.clear();
    QVERIFY(model.isEmpty());
    QCOMPARE(model.rowCount(), 0);
}

QTEST_GUILESS_MAIN(RepositoryTreeModelTest)

#include "repositorytreemodel_test.moc"
