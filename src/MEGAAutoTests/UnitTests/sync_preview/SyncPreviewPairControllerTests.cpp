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