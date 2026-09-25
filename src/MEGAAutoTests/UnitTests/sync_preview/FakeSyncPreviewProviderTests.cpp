#include "FakeSyncPreviewProvider.h"
#include "SyncPreviewTree.h"

#include <catch.hpp>

using namespace SyncPreview;

TEST_CASE("FakeTreeBuilder creates ancestors for nested files")
{
    FakeTreeBuilder builder;
    builder.addFile(QStringLiteral("deep/l2/l3/file.txt"), 10, 100, "tag");

    const Tree tree = builder.build();

    REQUIRE(tree.size() == 4);  // deep, deep/l2, deep/l2/l3, deep/l2/l3/file.txt
    CHECK(tree.contains(QStringLiteral("deep")));
    CHECK(tree.contains(QStringLiteral("deep/l2")));
    CHECK(tree.contains(QStringLiteral("deep/l2/l3")));
    CHECK(tree.contains(QStringLiteral("deep/l2/l3/file.txt")));

    const Entry* file = tree.find(QStringLiteral("deep/l2/l3/file.txt"));
    REQUIRE(file != nullptr);
    CHECK(file->type == EntryType::File);
    CHECK(file->size == 10);
    CHECK(file->modifiedTime == 100);
    CHECK(file->contentHash == QByteArray("tag"));
}

TEST_CASE("FakeTreeBuilder does not duplicate existing folders")
{
    FakeTreeBuilder builder;
    builder.addFolder(QStringLiteral("a"));
    builder.addFolder(QStringLiteral("a"));
    builder.addFile(QStringLiteral("a/f.txt"), 1, 1, "x");

    const Tree tree = builder.build();

    REQUIRE(tree.size() == 2);  // a, a/f.txt
}

TEST_CASE("FakeSideProvider implements both sides of the seam")
{
    FakeTreeBuilder builder;
    builder.addFile(QStringLiteral("a.txt"), 1, 1, "x");
    const Tree tree = builder.build();

    FakeSideProvider provider(tree);
    LocalSideProvider& local = provider;
    RemoteSideProvider& remote = provider;

    CHECK(local.snapshot().size() == 1);
    CHECK(remote.snapshot().size() == 1);
    CHECK(local.snapshot().find(QStringLiteral("a.txt")) != nullptr);
}

TEST_CASE("edgeCaseKitchenSink covers every required edge case")
{
    const FakeScenario scenario = FakeScenarios::edgeCaseKitchenSink();

    // Nested directories.
    CHECK(scenario.local.contains(QStringLiteral("docs/plans/q1.txt")));
    CHECK(scenario.remote.contains(QStringLiteral("docs/plans/q1.txt")));

    // Empty dirs on both sides.
    CHECK(scenario.local.find(QStringLiteral("empty_local")) != nullptr);
    CHECK(scenario.remote.find(QStringLiteral("empty_remote")) != nullptr);

    // Same name, different content (CRC-only mismatch with equal sizes).
    const Entry* img1Local = scenario.local.find(QStringLiteral("photos/img1.png"));
    const Entry* img1Remote = scenario.remote.find(QStringLiteral("photos/img1.png"));
    REQUIRE(img1Local != nullptr);
    REQUIRE(img1Remote != nullptr);
    CHECK(img1Local->size == img1Remote->size);
    CHECK(img1Local->contentHash != img1Remote->contentHash);

    // Same name, different content (size mismatch exercises the
    // short-circuit).
    const Entry* img2Local = scenario.local.find(QStringLiteral("photos/img2.png"));
    const Entry* img2Remote = scenario.remote.find(QStringLiteral("photos/img2.png"));
    REQUIRE(img2Local != nullptr);
    REQUIRE(img2Remote != nullptr);
    CHECK(img2Local->size != img2Remote->size);

    // Same content, different names.
    const Entry* twinLocal = scenario.local.find(QStringLiteral("archive/a.txt"));
    const Entry* twinRemote = scenario.remote.find(QStringLiteral("old/b.txt"));
    REQUIRE(twinLocal != nullptr);
    REQUIRE(twinRemote != nullptr);
    CHECK(twinLocal->size == twinRemote->size);
    CHECK(twinLocal->contentHash == twinRemote->contentHash);

    // File-vs-folder type mismatch (with a child inside the remote folder).
    const Entry* mismatchLocal = scenario.local.find(QStringLiteral("misc/notes.txt"));
    const Entry* mismatchRemote = scenario.remote.find(QStringLiteral("misc/notes.txt"));
    REQUIRE(mismatchLocal != nullptr);
    REQUIRE(mismatchRemote != nullptr);
    CHECK(mismatchLocal->type == EntryType::File);
    CHECK(mismatchRemote->type == EntryType::Folder);
    CHECK(scenario.remote.contains(QStringLiteral("misc/notes.txt/draft.txt")));

    // Case-insensitive name collision.
    CHECK(scenario.local.contains(QStringLiteral("notes/A.txt")));
    CHECK(scenario.remote.contains(QStringLiteral("notes/a.txt")));

    // Single-sided files.
    CHECK(scenario.local.contains(QStringLiteral("only_local.txt")));
    CHECK(scenario.remote.contains(QStringLiteral("only_remote.txt")));

    // Deep identical tree (five levels).
    CHECK(scenario.local.contains(QStringLiteral("deep/l2/l3/l4/l5/deep.txt")));
    CHECK(scenario.remote.contains(QStringLiteral("deep/l2/l3/l4/l5/deep.txt")));
}

TEST_CASE("deepTree generates the requested depth")
{
    const FakeScenario scenario = FakeScenarios::deepTree(6);

    const QString leaf = QStringLiteral("level1/level2/level3/level4/level5/level6/leaf.txt");
    CHECK(scenario.local.contains(leaf));
    CHECK(scenario.remote.contains(leaf));
    CHECK(scenario.local.size() == 7);
    CHECK(scenario.remote.size() == 7);
}

TEST_CASE("emptySides produces empty trees")
{
    const FakeScenario scenario = FakeScenarios::emptySides();

    CHECK(scenario.local.isEmpty());
    CHECK(scenario.remote.isEmpty());
}

TEST_CASE("renameSwap pins the moved-content edge case")
{
    const FakeScenario scenario = FakeScenarios::renameSwap();

    // foo.txt paired on both sides with different content; bar.txt holds
    // the LOCAL foo.txt content under a new name on the remote side.
    REQUIRE(scenario.local.find(QStringLiteral("foo.txt")) != nullptr);
    REQUIRE(scenario.remote.find(QStringLiteral("foo.txt")) != nullptr);
    REQUIRE(scenario.remote.find(QStringLiteral("bar.txt")) != nullptr);
    CHECK(scenario.local.find(QStringLiteral("foo.txt"))->contentHash !=
          scenario.remote.find(QStringLiteral("foo.txt"))->contentHash);
    CHECK(scenario.local.find(QStringLiteral("foo.txt"))->contentHash ==
          scenario.remote.find(QStringLiteral("bar.txt"))->contentHash);
    CHECK_FALSE(scenario.local.contains(QStringLiteral("bar.txt")));
}

TEST_CASE("Tree lookup is exact and case-sensitive")
{
    FakeTreeBuilder builder;
    builder.addFile(QStringLiteral("notes/A.txt"), 1, 1, "x");
    const Tree tree = builder.build();

    CHECK(tree.contains(QStringLiteral("notes/A.txt")));
    CHECK_FALSE(tree.contains(QStringLiteral("notes/a.txt")));
    CHECK(tree.find(QStringLiteral("notes/a.txt")) == nullptr);
}
