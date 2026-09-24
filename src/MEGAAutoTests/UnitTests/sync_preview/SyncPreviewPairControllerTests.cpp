#include "FakeSyncPreviewProvider.h"
#include "SyncPreviewPairController.h"

#include <QTemporaryDir>

#include <catch.hpp>

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

    // Deciding both conflict rows keeps the same one-upload/one-download
    // shape; folder nodes carry no bytes.
    controller.setAction(pairId, QStringLiteral("old/b.txt"), Action::LocalToRemote);
    controller.setAction(pairId, QStringLiteral("twin/x.txt"), Action::RemoteToLocal);

    const PairSummary delta = controller.summary(pairId);
    CHECK(delta.localBytes == 40);
    CHECK(delta.remoteBytes == 40);
    CHECK(delta.pendingRemoteBytes == 30);  // upload of old/b.txt (30)
    CHECK(delta.pendingRemoteFiles == 1);
    CHECK(delta.pendingLocalBytes == 30);   // download of twin/x.txt (30)
    CHECK(delta.pendingLocalFiles == 1);
    CHECK(delta.pendingLocalRemoved == 0);
    CHECK(delta.pendingRemoteRemoved == 0);

    // A "do nothing" decision on one conflict row removes it from the delta.
    controller.setAction(pairId, QStringLiteral("old/b.txt"), Action::None);
    const PairSummary afterNone = controller.summary(pairId);
    CHECK(afterNone.pendingRemoteBytes == 0);
    CHECK(afterNone.pendingRemoteFiles == 0);
    CHECK(afterNone.pendingLocalBytes == 30);
    CHECK(afterNone.pendingLocalFiles == 1);

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