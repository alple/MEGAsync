#include "FakeSyncPreviewProvider.h"
#include "SyncPreviewClassifier.h"

#include <catch.hpp>

using namespace SyncPreview;

namespace
{
    Classification classify(const FakeScenario& scenario)
    {
        const Classifier classifier;
        const FakeSideProvider localProvider(scenario.local);
        const FakeSideProvider remoteProvider(scenario.remote);
        return classifier.classify(localProvider, remoteProvider);
    }

    const Row* findRow(const Classification& classification, const QString& path)
    {
        return classification.find(path);
    }
}

TEST_CASE("Identical files are classified by size short-circuit and CRC")
{
    FakeTreeBuilder localBuilder;
    FakeTreeBuilder remoteBuilder;

    // Same size, same CRC -> identical.
    localBuilder.addFile(QStringLiteral("same.txt"), 100, 1000, "content");
    remoteBuilder.addFile(QStringLiteral("same.txt"), 100, 2000, "content");

    // Same size, different CRC -> both-differ.
    localBuilder.addFile(QStringLiteral("crcdiff.txt"), 200, 1000, "one");
    remoteBuilder.addFile(QStringLiteral("crcdiff.txt"), 200, 1000, "two");

    // Different size, same CRC -> both-differ (size short-circuit).
    localBuilder.addFile(QStringLiteral("sizediff.txt"), 300, 1000, "shared");
    remoteBuilder.addFile(QStringLiteral("sizediff.txt"), 400, 1000, "shared");

    // Missing CRC on one side -> different (conservative).
    localBuilder.addFile(QStringLiteral("nocrc.txt"), 50, 1000, "x");
    remoteBuilder.addFile(QStringLiteral("nocrc.txt"), 50, 1000, "");

    const Classification classification = classify({localBuilder.build(), remoteBuilder.build()});

    const Row* same = findRow(classification, QStringLiteral("same.txt"));
    REQUIRE(same != nullptr);
    CHECK(same->kind == RowKind::Identical);
    CHECK_FALSE(same->requiresApproval);
    CHECK(same->recommendedAction == Action::None);

    const Row* crcDiff = findRow(classification, QStringLiteral("crcdiff.txt"));
    REQUIRE(crcDiff != nullptr);
    CHECK(crcDiff->kind == RowKind::BothDiffer);
    CHECK(crcDiff->requiresApproval);

    const Row* sizeDiff = findRow(classification, QStringLiteral("sizediff.txt"));
    REQUIRE(sizeDiff != nullptr);
    CHECK(sizeDiff->kind == RowKind::BothDiffer);

    const Row* noCrc = findRow(classification, QStringLiteral("nocrc.txt"));
    REQUIRE(noCrc != nullptr);
    CHECK(noCrc->kind == RowKind::BothDiffer);
}

TEST_CASE("Both-differ rows recommend the newer side, ties recommend L->R")
{
    FakeTreeBuilder localBuilder;
    FakeTreeBuilder remoteBuilder;

    localBuilder.addFile(QStringLiteral("localNewer.txt"), 10, 2000, "a");
    remoteBuilder.addFile(QStringLiteral("localNewer.txt"), 10, 1000, "b");

    localBuilder.addFile(QStringLiteral("remoteNewer.txt"), 10, 1000, "a");
    remoteBuilder.addFile(QStringLiteral("remoteNewer.txt"), 10, 2000, "b");

    localBuilder.addFile(QStringLiteral("tie.txt"), 10, 1000, "a");
    remoteBuilder.addFile(QStringLiteral("tie.txt"), 10, 1000, "b");

    const Classification classification = classify({localBuilder.build(), remoteBuilder.build()});

    CHECK(findRow(classification, QStringLiteral("localNewer.txt"))->recommendedAction == Action::LocalToRemote);
    CHECK(findRow(classification, QStringLiteral("remoteNewer.txt"))->recommendedAction == Action::RemoteToLocal);
    CHECK(findRow(classification, QStringLiteral("tie.txt"))->recommendedAction == Action::LocalToRemote);
}

TEST_CASE("Same-content-different-name pairs stay separate conflict rows")
{
    const Classification classification = classify(FakeScenarios::edgeCaseKitchenSink());

    // The local side of the twin pair.
    const Row* localTwin = findRow(classification, QStringLiteral("archive/a.txt"));
    REQUIRE(localTwin != nullptr);
    CHECK(localTwin->kind == RowKind::Conflict);
    CHECK(localTwin->local.has_value());
    CHECK_FALSE(localTwin->remote.has_value());
    CHECK(localTwin->hasIdenticalTwin);
    CHECK(localTwin->twinPath == QStringLiteral("old/b.txt"));
    CHECK(localTwin->requiresApproval);
    CHECK(localTwin->recommendedAction == Action::None);

    // The remote side: a separate, independently decidable row.
    const Row* remoteTwin = findRow(classification, QStringLiteral("old/b.txt"));
    REQUIRE(remoteTwin != nullptr);
    CHECK(remoteTwin->kind == RowKind::Conflict);
    CHECK(remoteTwin->remote.has_value());
    CHECK_FALSE(remoteTwin->local.has_value());
    CHECK(remoteTwin->hasIdenticalTwin);
    CHECK(remoteTwin->twinPath == QStringLiteral("archive/a.txt"));
    CHECK(remoteTwin->requiresApproval);
    CHECK(remoteTwin->recommendedAction == Action::None);
}

TEST_CASE("File-vs-folder mismatch is a blocker with flagged descendants")
{
    const Classification classification = classify(FakeScenarios::edgeCaseKitchenSink());

    const Row* mismatch = findRow(classification, QStringLiteral("misc/notes.txt"));
    REQUIRE(mismatch != nullptr);
    CHECK(mismatch->kind == RowKind::Blocker);
    CHECK(mismatch->blockerReason == BlockerReason::TypeMismatch);
    CHECK(mismatch->local.has_value());
    CHECK(mismatch->local->type == EntryType::File);
    CHECK(mismatch->remote.has_value());
    CHECK(mismatch->remote->type == EntryType::Folder);
    CHECK(mismatch->requiresApproval);
    CHECK(mismatch->recommendedAction == Action::None);

    // The remote folder's child surfaces under the blocked path.
    const Row* child = findRow(classification, QStringLiteral("misc/notes.txt/draft.txt"));
    REQUIRE(child != nullptr);
    CHECK(child->kind == RowKind::RemoteOnly);
    CHECK(child->underBlockedPath);
    CHECK(child->requiresApproval);
}

TEST_CASE("Case-insensitive name collisions merge into one blocker row")
{
    const Classification classification = classify(FakeScenarios::edgeCaseKitchenSink());

    const Row* collision = findRow(classification, QStringLiteral("notes/A.txt"));
    REQUIRE(collision != nullptr);
    CHECK(collision->kind == RowKind::Blocker);
    CHECK(collision->blockerReason == BlockerReason::CaseInsensitiveNameCollision);
    CHECK(collision->local.has_value());
    CHECK(collision->remote.has_value());
    CHECK(collision->requiresApproval);
    CHECK(collision->recommendedAction == Action::None);

    // The remote spelling is part of the same (merged) row.
    CHECK(findRow(classification, QStringLiteral("notes/a.txt")) == nullptr);
}

TEST_CASE("Single-sided rows and empty directories are classified with recommendations")
{
    const Classification classification = classify(FakeScenarios::edgeCaseKitchenSink());

    const Row* onlyLocal = findRow(classification, QStringLiteral("only_local.txt"));
    REQUIRE(onlyLocal != nullptr);
    CHECK(onlyLocal->kind == RowKind::LocalOnly);
    CHECK(onlyLocal->local.has_value());
    CHECK_FALSE(onlyLocal->remote.has_value());
    CHECK_FALSE(onlyLocal->requiresApproval);
    CHECK(onlyLocal->recommendedAction == Action::LocalToRemote);

    const Row* onlyRemote = findRow(classification, QStringLiteral("only_remote.txt"));
    REQUIRE(onlyRemote != nullptr);
    CHECK(onlyRemote->kind == RowKind::RemoteOnly);
    CHECK(onlyRemote->recommendedAction == Action::RemoteToLocal);

    const Row* emptyLocal = findRow(classification, QStringLiteral("empty_local"));
    REQUIRE(emptyLocal != nullptr);
    CHECK(emptyLocal->kind == RowKind::LocalOnly);
    CHECK(emptyLocal->local->type == EntryType::Folder);
    CHECK(emptyLocal->recommendedAction == Action::LocalToRemote);

    const Row* emptyRemote = findRow(classification, QStringLiteral("empty_remote"));
    REQUIRE(emptyRemote != nullptr);
    CHECK(emptyRemote->kind == RowKind::RemoteOnly);
    CHECK(emptyRemote->remote->type == EntryType::Folder);
    CHECK(emptyRemote->recommendedAction == Action::RemoteToLocal);
}

TEST_CASE("Directory rows mirror their subtree state")
{
    const Classification classification = classify(FakeScenarios::edgeCaseKitchenSink());

    CHECK(findRow(classification, QStringLiteral("docs"))->kind == RowKind::Identical);
    CHECK(findRow(classification, QStringLiteral("docs/plans"))->kind == RowKind::Identical);
    CHECK(findRow(classification, QStringLiteral("deep"))->kind == RowKind::Identical);
    CHECK(findRow(classification, QStringLiteral("deep/l2/l3/l4/l5"))->kind == RowKind::Identical);

    CHECK(findRow(classification, QStringLiteral("photos"))->kind == RowKind::BothDiffer);
    CHECK(findRow(classification, QStringLiteral("photos"))->requiresApproval);

    // notes pairs exactly (same spelling) but its children collide.
    CHECK(findRow(classification, QStringLiteral("notes"))->kind == RowKind::BothDiffer);

    // misc pairs exactly (same spelling) but hosts the type mismatch.
    CHECK(findRow(classification, QStringLiteral("misc"))->kind == RowKind::BothDiffer);

    CHECK(findRow(classification, QStringLiteral("archive"))->kind == RowKind::LocalOnly);
    CHECK(findRow(classification, QStringLiteral("old"))->kind == RowKind::RemoteOnly);
}

TEST_CASE("All six row kinds appear on the kitchen-sink scenario")
{
    const Classification classification = classify(FakeScenarios::edgeCaseKitchenSink());

    bool kinds[6] = {false, false, false, false, false, false};
    for (const Row& row : classification.rows)
    {
        kinds[static_cast<int>(row.kind)] = true;
    }
    CHECK(kinds[static_cast<int>(RowKind::LocalOnly)]);
    CHECK(kinds[static_cast<int>(RowKind::RemoteOnly)]);
    CHECK(kinds[static_cast<int>(RowKind::Identical)]);
    CHECK(kinds[static_cast<int>(RowKind::BothDiffer)]);
    CHECK(kinds[static_cast<int>(RowKind::Conflict)]);
    CHECK(kinds[static_cast<int>(RowKind::Blocker)]);
}

TEST_CASE("Row order is stable: local tree order, then remote leftovers")
{
    const Classification classification = classify(FakeScenarios::edgeCaseKitchenSink());

    REQUIRE(classification.rows.size() > 3);
    CHECK(classification.rows.first().relativePath == QStringLiteral("docs"));
    // Remote leftovers come last, in remote tree order:
    // old, misc/notes.txt/draft.txt, empty_remote, only_remote.txt.
    CHECK(classification.rows.last().relativePath == QStringLiteral("only_remote.txt"));
    CHECK(classification.rows.at(classification.rows.size() - 2).relativePath == QStringLiteral("empty_remote"));
}

TEST_CASE("Empty sides produce no rows")
{
    const Classification classification = classify(FakeScenarios::emptySides());

    CHECK(classification.rows.isEmpty());
}

TEST_CASE("Identical deep trees classify entirely as identical")
{
    const Classification classification = classify(FakeScenarios::deepTree(5));

    CHECK(classification.rows.size() == 6);  // 5 folders + 1 file
    for (const Row& row : classification.rows)
    {
        CHECK(row.kind == RowKind::Identical);
        CHECK_FALSE(row.requiresApproval);
    }
}
