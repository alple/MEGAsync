#include "FakeSyncPreviewProvider.h"
#include "SyncPreviewClassifier.h"
#include "SyncPreviewFakeApplier.h"
#include "SyncPreviewPlanner.h"

#include <QHash>

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

    // Classifies the scenario, plans it under `decisions` (recommended
    // actions included), applies the plan to a copy of the scenario and
    // re-classifies the mutated trees.
    Classification applyDecision(const FakeScenario& scenario, const QHash<QString, Action>& decisions)
    {
        const Plan plan = Planner().plan(classify(scenario), decisions);
        FakeScenario mutated = scenario;
        applyPlan(mutated, plan);
        return classify(mutated);
    }
}

TEST_CASE("Fake applier transfers single entries with ancestors")
{
    FakeScenario scenario;
    scenario.local = FakeTreeBuilder()
                         .addFolder(QStringLiteral("sub"))
                         .addFile(QStringLiteral("sub/only.txt"), 10, 100, "h")
                         .build();

    QHash<QString, Action> decisions;
    decisions.insert(QStringLiteral("sub/only.txt"), Action::LocalToRemote);
    const Classification after = applyDecision(scenario, decisions);

    const Row* only = after.find(QStringLiteral("sub/only.txt"));
    REQUIRE(only != nullptr);
    CHECK(only->kind == RowKind::Identical);
    REQUIRE(only->remote.has_value());
    CHECK(only->remote->size == 10);
    CHECK(only->remote->contentHash == QByteArray("h"));
    // The source copy stays put.
    REQUIRE(only->local.has_value());
    CHECK(only->local->contentHash == QByteArray("h"));
}

TEST_CASE("Fake applier trashes local-only content on R->L")
{
    FakeScenario scenario;
    scenario.local = FakeTreeBuilder()
                         .addFile(QStringLiteral("only.txt"), 10, 100, "h")
                         .build();

    QHash<QString, Action> decisions;
    decisions.insert(QStringLiteral("only.txt"), Action::RemoteToLocal);
    const Classification after = applyDecision(scenario, decisions);

    CHECK(after.rows.isEmpty());
}

TEST_CASE("Fake applier replaces paired content with the source side")
{
    FakeScenario scenario;
    scenario.local = FakeTreeBuilder().addFile(QStringLiteral("f.txt"), 100, 1000, "one").build();
    scenario.remote = FakeTreeBuilder().addFile(QStringLiteral("f.txt"), 100, 2000, "two").build();

    QHash<QString, Action> decisions;
    decisions.insert(QStringLiteral("f.txt"), Action::LocalToRemote);
    const Classification uploaded = applyDecision(scenario, decisions);
    REQUIRE(uploaded.find(QStringLiteral("f.txt")) != nullptr);
    CHECK(uploaded.find(QStringLiteral("f.txt"))->kind == RowKind::Identical);

    decisions.clear();
    decisions.insert(QStringLiteral("f.txt"), Action::RemoteToLocal);
    const Classification downloaded = applyDecision(scenario, decisions);
    REQUIRE(downloaded.find(QStringLiteral("f.txt")) != nullptr);
    CHECK(downloaded.find(QStringLiteral("f.txt"))->kind == RowKind::Identical);
}

TEST_CASE("Fake applier uploads a folder subtree on the recommended cascade")
{
    FakeScenario scenario;
    scenario.local = FakeTreeBuilder()
                         .addFolder(QStringLiteral("sub"))
                         .addFile(QStringLiteral("sub/f1.bin"), 100, 100, "f1")
                         .addFile(QStringLiteral("sub/f2.bin"), 50, 100, "f2")
                         .build();

    const Classification after = applyDecision(scenario, {});

    REQUIRE(after.rows.size() == 3);  // sub, sub/f1.bin, sub/f2.bin
    for (const Row& row : after.rows)
    {
        CHECK(row.kind == RowKind::Identical);
    }
}

TEST_CASE("Applying the conflict adopt renames the twin and clears both rows")
{
    FakeScenario scenario;
    scenario.local = FakeTreeBuilder()
                         .addFolder(QStringLiteral("old"))
                         .addFile(QStringLiteral("old/b.txt"), 30, 300, "twin")
                         .build();
    scenario.remote = FakeTreeBuilder()
                          .addFolder(QStringLiteral("twin"))
                          .addFile(QStringLiteral("twin/x.txt"), 30, 400, "twin")
                          .build();

    QHash<QString, Action> decisions;
    decisions.insert(QStringLiteral("old/b.txt"), Action::LocalToRemote);
    const Classification after = applyDecision(scenario, decisions);

    // The twin content now sits at old/b.txt on both sides; the remote
    // twin row is gone (its content moved). The emptied twin folder was
    // materialized locally by the node-only directory download, so it
    // re-pairs as identical.
    const Row* b = after.find(QStringLiteral("old/b.txt"));
    REQUIRE(b != nullptr);
    CHECK(b->kind == RowKind::Identical);
    CHECK(after.find(QStringLiteral("twin/x.txt")) == nullptr);
    const Row* twinFolder = after.find(QStringLiteral("twin"));
    REQUIRE(twinFolder != nullptr);
    CHECK(twinFolder->kind == RowKind::Identical);
}

TEST_CASE("Applying the rename-swap resolution loses no content")
{
    const FakeScenario scenario = FakeScenarios::renameSwap();

    QHash<QString, Action> decisions;
    decisions.insert(QStringLiteral("foo.txt"), Action::LocalToRemote);
    const Classification after = applyDecision(scenario, decisions);

    // foo.txt matches on both sides (the adopted twin content); the
    // remote edit survives under bar.txt.
    const Row* foo = after.find(QStringLiteral("foo.txt"));
    REQUIRE(foo != nullptr);
    CHECK(foo->kind == RowKind::Identical);
    REQUIRE(foo->remote.has_value());
    CHECK(foo->remote->contentHash == QByteArray("hash_1"));
    REQUIRE(foo->local.has_value());
    CHECK(foo->local->contentHash == QByteArray("hash_1"));

    const Row* bar = after.find(QStringLiteral("bar.txt"));
    REQUIRE(bar != nullptr);
    CHECK(bar->kind == RowKind::RemoteOnly);
    REQUIRE(bar->remote.has_value());
    CHECK(bar->remote->contentHash == QByteArray("hash_2"));

    // Both copies still exist: nothing was overwritten away.
    CHECK(after.rows.size() == 2);
}

TEST_CASE("Applying the double-rename swap resolves the R->L direction too")
{
    const FakeScenario scenario = FakeScenarios::renameChain();

    QHash<QString, Action> decisions;
    decisions.insert(QStringLiteral("foo.txt"), Action::RemoteToLocal);
    const Classification after = applyDecision(scenario, decisions);

    // foo.txt matches on both sides (the local twin adopted the remote
    // content); the displaced local content survives under foo2.txt —
    // which the re-scan reads as a fresh same-content twin pair with the
    // remote bar.txt (no automatic transfer happens: the twin-flagged
    // rows recommend no default, the reviewer decides next round).
    const Row* foo = after.find(QStringLiteral("foo.txt"));
    REQUIRE(foo != nullptr);
    CHECK(foo->kind == RowKind::Identical);
    CHECK(foo->remote->contentHash == QByteArray("hash_2"));

    const Row* foo2 = after.find(QStringLiteral("foo2.txt"));
    REQUIRE(foo2 != nullptr);
    CHECK(foo2->kind == RowKind::Conflict);
    CHECK(foo2->twinPath == QStringLiteral("bar.txt"));

    const Row* bar = after.find(QStringLiteral("bar.txt"));
    REQUIRE(bar != nullptr);
    CHECK(bar->kind == RowKind::Conflict);
    CHECK(bar->twinPath == QStringLiteral("foo2.txt"));
    CHECK(bar->remote->contentHash == QByteArray("hash_1"));
}

TEST_CASE("Applying the cross-folder swap moves content across folders")
{
    const FakeScenario scenario = FakeScenarios::renameCrossFolder();

    QHash<QString, Action> decisions;
    decisions.insert(QStringLiteral("docs/a.txt"), Action::LocalToRemote);
    const Classification after = applyDecision(scenario, decisions);

    const Row* a = after.find(QStringLiteral("docs/a.txt"));
    REQUIRE(a != nullptr);
    CHECK(a->kind == RowKind::Identical);
    CHECK(a->remote->contentHash == QByteArray("hash_1"));

    // The displaced remote content lands in the vacated twin folder.
    const Row* archive = after.find(QStringLiteral("archive/a.txt"));
    REQUIRE(archive != nullptr);
    CHECK(archive->kind == RowKind::RemoteOnly);
    CHECK(archive->remote->contentHash == QByteArray("hash_2"));
}

TEST_CASE("Applying a blocker resolution displaces the colliding entry and transfers")
{
    // Case-insensitive collision: local notes/A.txt, remote notes/a.txt.
    FakeScenario collision;
    collision.local = FakeTreeBuilder().addFile(QStringLiteral("notes/A.txt"), 60, 600, "cl-local").build();
    collision.remote = FakeTreeBuilder().addFile(QStringLiteral("notes/a.txt"), 70, 600, "cl-remote").build();

    QHash<QString, Action> decisions;
    decisions.insert(QStringLiteral("notes/A.txt"), Action::LocalToRemote);
    const Classification uploaded = applyDecision(collision, decisions);

    const Row* a = uploaded.find(QStringLiteral("notes/A.txt"));
    REQUIRE(a != nullptr);
    CHECK(a->kind == RowKind::Identical);
    const Row* displaced = uploaded.find(QStringLiteral("notes/a (1).txt"));
    REQUIRE(displaced != nullptr);
    CHECK(displaced->kind == RowKind::RemoteOnly);
    CHECK(displaced->remote->contentHash == QByteArray("cl-remote"));

    // The reverse direction displaces the local spelling instead.
    decisions.clear();
    decisions.insert(QStringLiteral("notes/A.txt"), Action::RemoteToLocal);
    const Classification downloaded = applyDecision(collision, decisions);

    const Row* remoteSpelling = downloaded.find(QStringLiteral("notes/a.txt"));
    REQUIRE(remoteSpelling != nullptr);
    CHECK(remoteSpelling->kind == RowKind::Identical);
    const Row* localDisplaced = downloaded.find(QStringLiteral("notes/A (1).txt"));
    REQUIRE(localDisplaced != nullptr);
    CHECK(localDisplaced->kind == RowKind::LocalOnly);
    CHECK(localDisplaced->local->contentHash == QByteArray("cl-local"));
}

TEST_CASE("Applying the type-mismatch blocker keeps folder names whole")
{
    FakeScenario scenario;
    scenario.local = FakeTreeBuilder().addFile(QStringLiteral("notes.txt"), 10, 100, "nl").build();
    scenario.remote = FakeTreeBuilder()
                          .addFolder(QStringLiteral("notes.txt"))
                          .addFile(QStringLiteral("notes.txt/draft.txt"), 5, 100, "d")
                          .build();

    QHash<QString, Action> decisions;
    decisions.insert(QStringLiteral("notes.txt"), Action::LocalToRemote);
    const Classification uploaded = applyDecision(scenario, decisions);

    // The remote folder moved aside (whole name, no extension split) and
    // the local file landed at the freed path.
    const Row* notes = uploaded.find(QStringLiteral("notes.txt"));
    REQUIRE(notes != nullptr);
    CHECK(notes->kind == RowKind::Identical);
    CHECK(notes->remote->type == EntryType::File);
    const Row* displacedChild = uploaded.find(QStringLiteral("notes.txt (1)/draft.txt"));
    REQUIRE(displacedChild != nullptr);
    CHECK(displacedChild->kind == RowKind::RemoteOnly);

    // R->L: the local file moves aside (extension kept), the remote folder
    // downloads at the freed path (its child follows via its own row).
    decisions.clear();
    decisions.insert(QStringLiteral("notes.txt"), Action::RemoteToLocal);
    const Classification downloaded = applyDecision(scenario, decisions);

    const Row* localDisplaced = downloaded.find(QStringLiteral("notes (1).txt"));
    REQUIRE(localDisplaced != nullptr);
    CHECK(localDisplaced->kind == RowKind::LocalOnly);
    CHECK(localDisplaced->local->contentHash == QByteArray("nl"));
    const Row* draft = downloaded.find(QStringLiteral("notes.txt/draft.txt"));
    REQUIRE(draft != nullptr);
    CHECK(draft->kind == RowKind::Identical);
    CHECK(draft->local.has_value());
}

TEST_CASE("The review loop applies repeatedly until the pair converges")
{
    // A local-only tree: the recommended plan uploads everything; the
    // second apply is a no-op and the classification reads fully identical.
    FakeScenario scenario = FakeScenarios::manyLocalOnlyFiles(4);

    const Classification firstPass = applyDecision(scenario, {});
    for (const Row& row : firstPass.rows)
    {
        CHECK(row.kind == RowKind::Identical);
    }

    const Classification secondPass = applyDecision(scenario, {});
    for (const Row& row : secondPass.rows)
    {
        CHECK(row.kind == RowKind::Identical);
    }
}