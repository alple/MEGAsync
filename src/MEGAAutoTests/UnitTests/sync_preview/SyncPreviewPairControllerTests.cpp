#include "FakeSyncPreviewProvider.h"
#include "SyncPreviewFakeApplier.h"
#include "SyncPreviewPairController.h"

#include <QTemporaryDir>

#include <catch.hpp>

#include <memory>

using namespace SyncPreview;

namespace
{
    // "demo-a": identical docs/a.txt plus a conflict twin pair
    // (old/b.txt <-> twin/x.txt). "demo-b": the same pair mutated so that
    // docs/a.txt differs and the twin is gone.
    FakeScenario scenarioA()
    {
        FakeScenario scenario;
        scenario.local = FakeTreeBuilder()
                             .addFolder(QStringLiteral("docs"))
                             .addFile(QStringLiteral("docs/a.txt"), 10, 100, "same")
                             .addFile(QStringLiteral("old/b.txt"), 30, 300, "twin")
                             .build();
        scenario.remote = FakeTreeBuilder()
                              .addFolder(QStringLiteral("docs"))
                              .addFile(QStringLiteral("docs/a.txt"), 10, 999, "same")
                              .addFile(QStringLiteral("twin/x.txt"), 30, 400, "twin")
                              .build();
        return scenario;
    }

    FakeScenario scenarioB()
    {
        FakeScenario scenario;
        scenario.local = FakeTreeBuilder()
                             .addFolder(QStringLiteral("docs"))
                             .addFile(QStringLiteral("docs/a.txt"), 10, 100, "same")
                             .addFile(QStringLiteral("old/b.txt"), 30, 300, "twin")
                             .build();
        scenario.remote = FakeTreeBuilder()
                              .addFolder(QStringLiteral("docs"))
                              .addFile(QStringLiteral("docs/a.txt"), 11, 999, "changed")
                              .build();
        return scenario;
    }

    SideProviderFactory factoryFor(const QString& label, const FakeScenario& scenario)
    {
        return [label, scenario](const Pair& pair) -> std::optional<PairSideProviders>
        {
            if (pair.localPath != label)
            {
                return std::nullopt;
            }
            return PairSideProviders{std::make_shared<FakeSideProvider>(scenario.local),
                                     std::make_shared<FakeSideProvider>(scenario.remote)};
        };
    }

    PairCandidate candidateA()
    {
        PairCandidate candidate;
        candidate.localPath = QStringLiteral("demo-a");
        candidate.remotePath = QStringLiteral("demo-a (remote)");
        return candidate;
    }
}

TEST_CASE("PairController restores an empty queue from a missing file")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    PairController controller{QueueFileStore(dir.filePath(QStringLiteral("queue.json")))};
    controller.setSideProviderFactory(factoryFor(QStringLiteral("demo-a"), scenarioA()));
    controller.restore();

    CHECK(controller.pairs().isEmpty());
}

TEST_CASE("PairController adds a pair, classifies it and persists it")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString filePath = dir.filePath(QStringLiteral("queue.json"));

    PairController controller{QueueFileStore(filePath)};
    controller.setSideProviderFactory(factoryFor(QStringLiteral("demo-a"), scenarioA()));
    controller.addPair(candidateA());

    REQUIRE(controller.pairs().size() == 1);
    const QString pairId = controller.pairs().first().id;
    // docs/, docs/a.txt, old/, old/b.txt (conflict), twin/, twin/x.txt (conflict).
    CHECK(controller.classification(pairId).rows.size() == 6);

    // The queue survived the controller: a fresh controller on the same file
    // restores the pair and re-scans it.
    PairController reloaded{QueueFileStore(filePath)};
    reloaded.setSideProviderFactory(factoryFor(QStringLiteral("demo-a"), scenarioA()));
    reloaded.restore();

    REQUIRE(reloaded.pairs().size() == 1);
    CHECK(reloaded.pairs().first().localPath == QStringLiteral("demo-a"));
    CHECK(reloaded.classification(reloaded.pairs().first().id).rows.size() == 6);
    CHECK(reloaded.reFlaggedPaths(reloaded.pairs().first().id).isEmpty());
}

TEST_CASE("PairController tracks decisions, approvals and the commit gate")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    PairController controller{QueueFileStore(dir.filePath(QStringLiteral("queue.json")))};
    controller.setSideProviderFactory(factoryFor(QStringLiteral("demo-a"), scenarioA()));
    controller.addPair(candidateA());
    const QString pairId = controller.pairs().first().id;

    // The conflict rows require approval: the gate is closed initially.
    CHECK_FALSE(controller.allApproved(pairId));

    // Approve every flagged row.
    const Classification& classification = controller.classification(pairId);
    QStringList flagged;
    for (const Row& row : classification.rows)
    {
        if (row.requiresApproval)
        {
            CHECK(row.kind == RowKind::Conflict);
            flagged.append(row.relativePath);
            controller.setApproved(pairId, row.relativePath, true);
        }
    }
    REQUIRE_FALSE(flagged.isEmpty());
    CHECK(controller.allApproved(pairId));

    // A decision made on an ordinary row.
    controller.setAction(pairId, QStringLiteral("docs/a.txt"), Action::LocalToRemote);
    // Identical rows do not require approval: the gate stays open.
    CHECK(controller.allApproved(pairId));

    const Plan plan = controller.plan(pairId);
    const RowPlan* a = plan.find(QStringLiteral("docs/a.txt"));
    REQUIRE(a);
    CHECK(a->action == Action::LocalToRemote);
    CHECK(a->actionSource == ActionSource::OwnDecision);

    // Decisions survive a reload; the approval gate reopens on restore only
    // if the classification changed (it did not here).
    const QString filePath = dir.filePath(QStringLiteral("queue.json"));
    PairController reloaded{QueueFileStore(filePath)};
    reloaded.setSideProviderFactory(factoryFor(QStringLiteral("demo-a"), scenarioA()));
    reloaded.restore();

    REQUIRE(reloaded.pairs().size() == 1);
    const QString reloadedId = reloaded.pairs().first().id;
    CHECK(reloaded.allApproved(reloadedId));
    const Plan reloadedPlan = reloaded.plan(reloadedId);
    const RowPlan* reloadedA = reloadedPlan.find(QStringLiteral("docs/a.txt"));
    REQUIRE(reloadedA);
    CHECK(reloadedA->action == Action::LocalToRemote);
}

TEST_CASE("PairController re-flags changed classifications on restore")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString filePath = dir.filePath(QStringLiteral("queue.json"));

    PairController first{QueueFileStore(filePath)};
    first.setSideProviderFactory(factoryFor(QStringLiteral("demo-a"), scenarioA()));
    first.addPair(candidateA());
    const QString pairId = first.pairs().first().id;
    first.setApproved(pairId, QStringLiteral("old/b.txt"), true);

    // The fake scenario for the same label changed: the twin is gone, so
    // old/b.txt is now local-only.
    PairController reloaded{QueueFileStore(filePath)};
    reloaded.setSideProviderFactory(factoryFor(QStringLiteral("demo-a"), scenarioB()));
    reloaded.restore();

    REQUIRE(reloaded.pairs().size() == 1);
    const QString reloadedId = reloaded.pairs().first().id;
    const QStringList reFlagged = reloaded.reFlaggedPaths(reloadedId);
    CHECK(reFlagged.contains(QStringLiteral("old/b.txt")));
    CHECK_FALSE(reloaded.allApproved(reloadedId));
}

TEST_CASE("PairController drops decisions for rows that vanished")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString filePath = dir.filePath(QStringLiteral("queue.json"));

    PairController first{QueueFileStore(filePath)};
    first.setSideProviderFactory(factoryFor(QStringLiteral("demo-a"), scenarioA()));
    first.addPair(candidateA());
    first.setAction(first.pairs().first().id, QStringLiteral("twin/x.txt"), Action::RemoteToLocal);

    PairController reloaded{QueueFileStore(filePath)};
    reloaded.setSideProviderFactory(factoryFor(QStringLiteral("demo-a"), scenarioB()));
    reloaded.restore();

    const QVector<RowDecision>& decisions = reloaded.pairs().first().decisions;
    for (const RowDecision& decision : decisions)
    {
        CHECK_FALSE(decision.relativePath == QStringLiteral("twin/x.txt"));
    }
}

TEST_CASE("PairController removes pairs and drops completed pairs on restore")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString filePath = dir.filePath(QStringLiteral("queue.json"));

    PairController controller{QueueFileStore(filePath)};
    controller.setSideProviderFactory(factoryFor(QStringLiteral("demo-a"), scenarioA()));
    controller.addPair(candidateA());
    const QString pairId = controller.pairs().first().id;

    // Remove by hand.
    controller.removePair(pairId);
    CHECK(controller.pairs().isEmpty());
    PairController reloaded{QueueFileStore(filePath)};
    reloaded.setSideProviderFactory(factoryFor(QStringLiteral("demo-a"), scenarioA()));
    reloaded.restore();
    CHECK(reloaded.pairs().isEmpty());

    // A completed pair (its sync was created) is dropped on restore.
    Queue completed;
    Pair done;
    done.id = QStringLiteral("done-pair");
    done.localPath = QStringLiteral("demo-a");
    done.remotePath = QStringLiteral("demo-a (remote)");
    done.remoteHandle = QStringLiteral("h");
    done.completed = true;
    completed.pairs.append(done);
    REQUIRE(QueueFileStore(filePath).save(completed));

    reloaded.restore();
    CHECK(reloaded.pairs().isEmpty());
}

TEST_CASE("PairController summarizes subtree stats and the pending delta")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    PairController controller{QueueFileStore(dir.filePath(QStringLiteral("queue.json")))};
    controller.setSideProviderFactory(factoryFor(QStringLiteral("demo-a"), scenarioA()));
    controller.addPair(candidateA());
    const QString pairId = controller.pairs().first().id;

    // Whole subtree: local docs/, docs/a.txt (10), old/, old/b.txt (30);
    // remote docs/, docs/a.txt (10), twin/, twin/x.txt (30).
    const PairSummary subtree = controller.summary(pairId);
    CHECK(subtree.localBytes == 40);
    CHECK(subtree.localFiles == 2);
    CHECK(subtree.localDirs == 2);
    CHECK(subtree.remoteBytes == 40);
    CHECK(subtree.remoteFiles == 2);
    CHECK(subtree.remoteDirs == 2);

    // No decisions yet, but the recommended plan already cascades the
    // single-sided directories: old/ uploads old/b.txt, twin/ downloads
    // twin/x.txt. The conflicts themselves stay undecided.
    CHECK(subtree.pendingLocalBytes == 30);   // download of twin/x.txt (30)
    CHECK(subtree.pendingLocalFiles == 1);
    CHECK(subtree.pendingLocalRemoved == 0);
    CHECK(subtree.pendingRemoteBytes == 30);  // upload of old/b.txt (30)
    CHECK(subtree.pendingRemoteFiles == 1);
    CHECK(subtree.pendingRemoteRemoved == 0);

    // Deciding both conflict rows (rename-aware semantics, MEGA-2.9): the
    // local row's arrow adopts the identical remote twin by renaming it to
    // old/b.txt — a rename transfers no bytes — and consumes the twin row,
    // whose own decision is superseded and plans nothing.
    controller.setAction(pairId, QStringLiteral("old/b.txt"), Action::LocalToRemote);
    controller.setAction(pairId, QStringLiteral("twin/x.txt"), Action::RemoteToLocal);

    const PairSummary delta = controller.summary(pairId);
    CHECK(delta.localBytes == 40);
    CHECK(delta.remoteBytes == 40);
    CHECK(delta.pendingRemoteBytes == 0);
    CHECK(delta.pendingRemoteFiles == 0);
    CHECK(delta.pendingLocalBytes == 0);
    CHECK(delta.pendingLocalFiles == 0);
    CHECK(delta.pendingLocalRemoved == 0);
    CHECK(delta.pendingRemoteRemoved == 0);

    // Dropping the local row's decision back to "do nothing" hands the
    // resolution to the remote row: its arrow now adopts the local twin by
    // renaming it to twin/x.txt — again a rename, still no bytes.
    controller.setAction(pairId, QStringLiteral("old/b.txt"), Action::None);
    const PairSummary afterNone = controller.summary(pairId);
    CHECK(afterNone.pendingRemoteBytes == 0);
    CHECK(afterNone.pendingRemoteFiles == 0);
    CHECK(afterNone.pendingLocalBytes == 0);
    CHECK(afterNone.pendingLocalFiles == 0);

    // Unknown pair id: zeros.
    const PairSummary none = controller.summary(QStringLiteral("no-such-pair"));
    CHECK(none.localBytes == 0);
    CHECK(none.localFiles == 0);
    CHECK(none.pendingRemoteRemoved == 0);
}

TEST_CASE("PairController summary dedupes cascaded directory consequences")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    // A local-only folder with two files; remote is empty.
    FakeScenario scenario;
    scenario.local = FakeTreeBuilder()
                         .addFolder(QStringLiteral("sub"))
                         .addFile(QStringLiteral("sub/f1.bin"), 100, 100, "f1")
                         .addFile(QStringLiteral("sub/f2.bin"), 50, 100, "f2")
                         .build();

    PairCandidate candidate;
    candidate.localPath = QStringLiteral("demo-c");
    candidate.remotePath = QStringLiteral("demo-c (remote)");

    PairController controller{QueueFileStore(dir.filePath(QStringLiteral("queue.json")))};
    controller.setSideProviderFactory(factoryFor(QStringLiteral("demo-c"), scenario));
    controller.addPair(candidate);
    const QString pairId = controller.pairs().first().id;

    // Recommended cascade (subtree upload on sub/): the directory row
    // aggregates both file rows plus its own folder node.
    PairSummary cascaded = controller.summary(pairId);
    CHECK(cascaded.pendingRemoteFiles == 2);
    CHECK(cascaded.pendingRemoteBytes == 150);
    CHECK(cascaded.pendingLocalFiles == 0);

    // An explicit decision on one descendant turns the directory op into a
    // node-only upload, so the directory row aggregates that descendant's
    // consequences AND the descendant row carries them itself: the dedupe
    // must count the file once.
    controller.setAction(pairId, QStringLiteral("sub/f1.bin"), Action::LocalToRemote);
    cascaded = controller.summary(pairId);
    CHECK(cascaded.pendingRemoteFiles == 2);
    CHECK(cascaded.pendingRemoteBytes == 150);
}

TEST_CASE("Explicit do-nothing decisions stay out of the commit gate")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    PairController controller{QueueFileStore(dir.filePath(QStringLiteral("queue.json")))};
    controller.setSideProviderFactory(factoryFor(QStringLiteral("demo-a"), scenarioA()));
    controller.addPair(candidateA());
    const QString pairId = controller.pairs().first().id;

    // Both conflict rows await: they are flagged and undecided (their
    // recommended action is None, which is not an explicit decision).
    const QStringList undecided = controller.awaitingApprovalPaths(pairId);
    REQUIRE(undecided.size() == 2);
    CHECK(undecided.contains(QStringLiteral("old/b.txt")));
    CHECK(undecided.contains(QStringLiteral("twin/x.txt")));

    // An explicit do-nothing decision on one row takes it out of the gate;
    // the undecided twin still blocks.
    controller.setAction(pairId, QStringLiteral("old/b.txt"), Action::None);
    const QStringList afterNone = controller.awaitingApprovalPaths(pairId);
    REQUIRE(afterNone.size() == 1);
    CHECK(afterNone.first() == QStringLiteral("twin/x.txt"));
    CHECK_FALSE(controller.allApproved(pairId));

    // A do-nothing inherited from a directory decision cascades the same
    // way: deciding the parent folder to none clears the child too.
    controller.setAction(pairId, QStringLiteral("twin"), Action::None);
    CHECK(controller.awaitingApprovalPaths(pairId).isEmpty());
    CHECK(controller.allApproved(pairId));

    // Approval itself still satisfies the gate for a decided transfer row.
    controller.setAction(pairId, QStringLiteral("old/b.txt"), Action::LocalToRemote);
    CHECK(controller.awaitingApprovalPaths(pairId).size() == 1);
    controller.setApproved(pairId, QStringLiteral("old/b.txt"), true);
    CHECK(controller.allApproved(pairId));
}

TEST_CASE("applyPlan mutates the fake data and re-verifies the pair")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    // One shared mutable scenario stands in for the dialog's fake-data
    // store: both the factory and the plan applier see the same trees.
    const auto shared = std::make_shared<FakeScenario>(scenarioA());

    PairController controller{QueueFileStore(dir.filePath(QStringLiteral("queue.json")))};
    controller.setSideProviderFactory(
        [shared](const Pair& pair) -> std::optional<PairSideProviders>
        {
            if (pair.localPath != QStringLiteral("demo-a"))
            {
                return std::nullopt;
            }
            return PairSideProviders{std::make_shared<FakeSideProvider>(shared->local),
                                     std::make_shared<FakeSideProvider>(shared->remote)};
        });
    controller.setPlanApplier(
        [shared](const QString&, const Plan& plan) -> bool
        {
            applyPlan(*shared, plan);
            return true;
        });

    controller.addPair(candidateA());
    const QString pairId = controller.pairs().first().id;

    // The adopt resolution: the twin content renames to old/b.txt.
    controller.setAction(pairId, QStringLiteral("old/b.txt"), Action::LocalToRemote);

    bool changed = false;
    QObject::connect(&controller, &PairController::pairChanged, [&changed](const QString&) { changed = true; });
    REQUIRE(controller.applyPlan(pairId));
    CHECK(changed);

    // The re-scan reads the applied trees: the adopt landed (old/b.txt
    // identical on both sides) and the vanished twin row lost its decision.
    const Classification& after = controller.classification(pairId);
    const Row* b = after.find(QStringLiteral("old/b.txt"));
    REQUIRE(b != nullptr);
    CHECK(b->kind == RowKind::Identical);
    CHECK(after.find(QStringLiteral("twin/x.txt")) == nullptr);

    const Pair* pair = controller.pair(pairId);
    REQUIRE(pair != nullptr);
    for (const RowDecision& decision : pair->decisions)
    {
        CHECK_FALSE(decision.relativePath == QStringLiteral("twin/x.txt"));
    }

    // The plan applier is optional: without it the loop reports failure.
    PairController bare{QueueFileStore(dir.filePath(QStringLiteral("bare.json")))};
    bare.addPair(candidateA());
    CHECK_FALSE(bare.applyPlan(bare.pairs().first().id));
}