#include "FakeSyncPreviewProvider.h"
#include "SyncPreviewClassifier.h"
#include "SyncPreviewQueue.h"
#include "SyncPreviewReconciler.h"

#include <QHash>

#include <catch.hpp>

using namespace SyncPreview;

namespace
{
    // Local: docs/a.txt (identical), docs/b.txt (both-differ), old/b.txt
    // (conflict twin of the remote twin/x.txt). Remote adds twin/x.txt.
    FakeScenario sampleScenario()
    {
        FakeScenario scenario;
        scenario.local = FakeTreeBuilder()
                             .addFolder(QStringLiteral("docs"))
                             .addFile(QStringLiteral("docs/a.txt"), 10, 100, "same")
                             .addFile(QStringLiteral("docs/b.txt"), 20, 200, "same-b")
                             .addFile(QStringLiteral("old/b.txt"), 30, 300, "twin")
                             .build();
        scenario.remote = FakeTreeBuilder()
                              .addFolder(QStringLiteral("docs"))
                              .addFile(QStringLiteral("docs/a.txt"), 10, 999, "same")
                              .addFile(QStringLiteral("docs/b.txt"), 20, 999, "differs-b")
                              .addFile(QStringLiteral("twin/x.txt"), 30, 400, "twin")
                              .build();
        return scenario;
    }

    Classification classify(const FakeScenario& scenario)
    {
        const Classifier classifier;
        const FakeSideProvider localProvider(scenario.local);
        const FakeSideProvider remoteProvider(scenario.remote);
        return classifier.classify(localProvider, remoteProvider);
    }

    Queue restoredQueue()
    {
        Queue queue;

        Pair pair;
        pair.id = QStringLiteral("pair-1");
        pair.localPath = QStringLiteral("/tmp/local");
        pair.remotePath = QStringLiteral("Cloud Drive/remote");
        pair.remoteHandle = QStringLiteral("h");

        RowDecision decided;
        decided.relativePath = QStringLiteral("docs/a.txt");
        decided.action = Action::LocalToRemote;
        decided.approved = false;
        decided.kind = static_cast<int>(RowKind::LocalOnly);
        decided.requiresApproval = false;

        RowDecision approvedConflict;
        approvedConflict.relativePath = QStringLiteral("old/b.txt");
        approvedConflict.action = Action::RemoteToLocal;
        approvedConflict.approved = true;
        approvedConflict.kind = static_cast<int>(RowKind::Conflict);
        approvedConflict.requiresApproval = true;

        pair.decisions = {approvedConflict, decided};
        queue.pairs.append(pair);
        return queue;
    }
}

TEST_CASE("Reconciler keeps unchanged decisions and refreshes snapshots")
{
    const Classification classification = classify(sampleScenario());
    QHash<QString, Classification> fresh;
    fresh.insert(QStringLiteral("pair-1"), classification);

    Queue queue = restoredQueue();
    // The decision on docs/a.txt was snapshotted as LocalOnly, but the fresh
    // classification sees identical content on both sides: the kind changes
    // to Identical, so it is re-flagged.
    queue.pairs.first().decisions[1].kind = static_cast<int>(RowKind::LocalOnly);

    const ReconcileResult result = Reconciler().reconcile(queue, fresh);

    REQUIRE(result.queue.pairs.size() == 1);
    REQUIRE(result.queue.pairs.first().decisions.size() == 2);

    const RowDecision& a = result.queue.pairs.first().decisions.at(1);
    CHECK(a.relativePath == QStringLiteral("docs/a.txt"));
    CHECK(a.kind == static_cast<int>(RowKind::Identical));
    CHECK_FALSE(a.requiresApproval);
    CHECK_FALSE(a.approved);  // re-flagged: kind changed
    CHECK(result.reFlaggedPaths.value(QStringLiteral("pair-1")).contains(QStringLiteral("docs/a.txt")));

    const RowDecision& conflict = result.queue.pairs.first().decisions.at(0);
    CHECK(conflict.kind == static_cast<int>(RowKind::Conflict));
    CHECK(conflict.requiresApproval);
    CHECK(conflict.approved);  // unchanged classification keeps its approval
    CHECK_FALSE(result.reFlaggedPaths.value(QStringLiteral("pair-1")).contains(QStringLiteral("old/b.txt")));
}

TEST_CASE("Reconciler drops decisions for vanished rows")
{
    const Classification classification = classify(sampleScenario());
    QHash<QString, Classification> fresh;
    fresh.insert(QStringLiteral("pair-1"), classification);

    Queue queue = restoredQueue();
    RowDecision gone;
    gone.relativePath = QStringLiteral("removed-by-the-user.txt");
    gone.action = Action::LocalToRemote;
    queue.pairs.first().decisions.append(gone);

    const ReconcileResult result = Reconciler().reconcile(queue, fresh);

    REQUIRE(result.queue.pairs.size() == 1);
    for (const RowDecision& decision : result.queue.pairs.first().decisions)
    {
        CHECK_FALSE(decision.relativePath == QStringLiteral("removed-by-the-user.txt"));
    }
    CHECK(result.queue.pairs.first().decisions.size() == 2);
}

TEST_CASE("Reconciler drops completed pairs")
{
    const Classification classification = classify(sampleScenario());
    QHash<QString, Classification> fresh;
    fresh.insert(QStringLiteral("pair-1"), classification);

    Queue queue = restoredQueue();
    queue.pairs.first().completed = true;

    Pair another;
    another.id = QStringLiteral("pair-2");
    another.localPath = QStringLiteral("l");
    another.remotePath = QStringLiteral("r");
    another.remoteHandle = QStringLiteral("h");
    queue.pairs.append(another);

    const ReconcileResult result = Reconciler().reconcile(queue, fresh);

    REQUIRE(result.queue.pairs.size() == 1);
    CHECK(result.queue.pairs.first().id == QStringLiteral("pair-2"));
}

TEST_CASE("Reconciler re-flags decisions without snapshots (v1 migration)")
{
    const Classification classification = classify(sampleScenario());
    QHash<QString, Classification> fresh;
    fresh.insert(QStringLiteral("pair-1"), classification);

    Queue queue = restoredQueue();
    for (RowDecision& decision : queue.pairs.first().decisions)
    {
        decision.kind = RowDecision::UNKNOWN_KIND;
        decision.requiresApproval = false;
    }

    const ReconcileResult result = Reconciler().reconcile(queue, fresh);

    // Every decision is re-flagged once (snapshots unknown), including the
    // previously approved conflict, but the decisions survive and gain
    // correct snapshots.
    const QStringList reFlagged = result.reFlaggedPaths.value(QStringLiteral("pair-1"));
    CHECK(reFlagged.contains(QStringLiteral("old/b.txt")));
    CHECK(reFlagged.contains(QStringLiteral("docs/a.txt")));
    REQUIRE(result.queue.pairs.first().decisions.size() == 2);
    const RowDecision& conflict = result.queue.pairs.first().decisions.at(0);
    CHECK(conflict.kind == static_cast<int>(RowKind::Conflict));
    CHECK(conflict.requiresApproval);
    CHECK_FALSE(conflict.approved);
}

TEST_CASE("Reconciler keeps pairs without a fresh classification unverified")
{
    Queue queue = restoredQueue();

    const ReconcileResult result = Reconciler().reconcile(queue, {});

    REQUIRE(result.queue.pairs.size() == 1);
    CHECK(result.queue.pairs.first().decisions.size() == 2);
    CHECK(result.reFlaggedPaths.isEmpty());
}