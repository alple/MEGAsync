#include "FakeSyncPreviewProvider.h"
#include "SyncPreviewClassifier.h"
#include "SyncPreviewPlanner.h"

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

    const PlannedOperation* findOperation(const RowPlan& rowPlan, OperationType type)
    {
        for (const PlannedOperation& operation : rowPlan.operations)
        {
            if (operation.type == type)
            {
                return &operation;
            }
        }
        return nullptr;
    }

    bool containsPath(const QStringList& list, const QString& path)
    {
        return list.contains(path);
    }
}

TEST_CASE("Local-only rows plan by action: upload, trash deletion, best-effort uploads")
{
    FakeTreeBuilder localBuilder;
    localBuilder.addFile(QStringLiteral("g.txt"), 10, 100, "h");
    const Classification classification = classify({localBuilder.build(), Tree()});

    const Planner planner;
    const QString path = QStringLiteral("g.txt");

    QHash<QString, Action> decisions;
    decisions.insert(path, Action::LocalToRemote);
    RowPlan upload = planner.plan(classification, decisions).rows.first();
    REQUIRE(findOperation(upload, OperationType::Upload) != nullptr);
    CHECK(containsPath(upload.createdRemote, path));

    decisions.clear();
    decisions.insert(path, Action::RemoteToLocal);
    RowPlan trash = planner.plan(classification, decisions).rows.first();
    REQUIRE(findOperation(trash, OperationType::DeleteLocalToTrash) != nullptr);
    CHECK(trash.operations.first().type == OperationType::DeleteLocalToTrash);
    CHECK(containsPath(trash.removedLocal, path));

    decisions.clear();
    decisions.insert(path, Action::BestEffort);
    RowPlan bestEffort = planner.plan(classification, decisions).rows.first();
    CHECK(findOperation(bestEffort, OperationType::Upload) != nullptr);
}

TEST_CASE("Remote-only rows plan by action: rubbish deletion, download, best-effort downloads")
{
    FakeTreeBuilder remoteBuilder;
    remoteBuilder.addFile(QStringLiteral("r.txt"), 10, 100, "h");
    const Classification classification = classify({Tree(), remoteBuilder.build()});

    const Planner planner;
    const QString path = QStringLiteral("r.txt");

    QHash<QString, Action> decisions;
    decisions.insert(path, Action::LocalToRemote);
    RowPlan rubbish = planner.plan(classification, decisions).rows.first();
    REQUIRE(findOperation(rubbish, OperationType::DeleteRemoteToRubbish) != nullptr);
    CHECK(containsPath(rubbish.removedRemote, path));

    decisions.clear();
    decisions.insert(path, Action::RemoteToLocal);
    RowPlan download = planner.plan(classification, decisions).rows.first();
    CHECK(findOperation(download, OperationType::Download) != nullptr);
    CHECK(containsPath(download.createdLocal, path));

    decisions.clear();
    decisions.insert(path, Action::BestEffort);
    RowPlan bestEffort = planner.plan(classification, decisions).rows.first();
    CHECK(findOperation(bestEffort, OperationType::Download) != nullptr);
}

TEST_CASE("Both-differ rows plan recoverable replaces; best-effort plans nothing")
{
    FakeTreeBuilder localBuilder;
    FakeTreeBuilder remoteBuilder;
    localBuilder.addFile(QStringLiteral("f.txt"), 100, 1000, "one");
    remoteBuilder.addFile(QStringLiteral("f.txt"), 100, 1000, "two");
    const Classification classification = classify({localBuilder.build(), remoteBuilder.build()});

    const Planner planner;
    const QString path = QStringLiteral("f.txt");

    QHash<QString, Action> decisions;
    decisions.insert(path, Action::LocalToRemote);
    RowPlan uploadReplace = planner.plan(classification, decisions).rows.first();
    REQUIRE(findOperation(uploadReplace, OperationType::UploadReplace) != nullptr);
    CHECK(containsPath(uploadReplace.changedRemote, path));

    decisions.clear();
    decisions.insert(path, Action::RemoteToLocal);
    RowPlan downloadReplace = planner.plan(classification, decisions).rows.first();
    REQUIRE(findOperation(downloadReplace, OperationType::DownloadReplace) != nullptr);
    CHECK(containsPath(downloadReplace.changedLocal, path));

    decisions.clear();
    decisions.insert(path, Action::BestEffort);
    RowPlan bestEffort = planner.plan(classification, decisions).rows.first();
    CHECK(bestEffort.operations.isEmpty());
    CHECK_FALSE(bestEffort.warnings.isEmpty());
}

TEST_CASE("Identical rows plan nothing under any action")
{
    FakeTreeBuilder localBuilder;
    FakeTreeBuilder remoteBuilder;
    localBuilder.addFile(QStringLiteral("same.txt"), 100, 1000, "content");
    remoteBuilder.addFile(QStringLiteral("same.txt"), 100, 1000, "content");
    const Classification classification = classify({localBuilder.build(), remoteBuilder.build()});

    const Planner planner;
    for (const Action action : {Action::LocalToRemote, Action::RemoteToLocal, Action::BestEffort})
    {
        QHash<QString, Action> decisions;
        decisions.insert(QStringLiteral("same.txt"), action);
        const Plan plan = planner.plan(classification, decisions);
        CHECK(plan.rows.first().operations.isEmpty());
    }
}

TEST_CASE("Blocker rows never produce operations and warn")
{
    const Classification classification = classify(FakeScenarios::edgeCaseKitchenSink());
    const Planner planner;
    const Plan plan = planner.plan(classification, {});

    const RowPlan* mismatch = plan.find(QStringLiteral("misc/notes.txt"));
    REQUIRE(mismatch != nullptr);
    CHECK(mismatch->operations.isEmpty());
    CHECK(mismatch->action == Action::None);
    CHECK_FALSE(mismatch->warnings.isEmpty());

    const RowPlan* collision = plan.find(QStringLiteral("notes/A.txt"));
    REQUIRE(collision != nullptr);
    CHECK(collision->operations.isEmpty());
    CHECK_FALSE(collision->warnings.isEmpty());
}

TEST_CASE("Transfers under a blocked path carry an advisory warning")
{
    const Classification classification = classify(FakeScenarios::edgeCaseKitchenSink());
    const Planner planner;

    QHash<QString, Action> decisions;
    decisions.insert(QStringLiteral("misc/notes.txt/draft.txt"), Action::RemoteToLocal);
    const Plan plan = planner.plan(classification, decisions);

    const RowPlan* draft = plan.find(QStringLiteral("misc/notes.txt/draft.txt"));
    REQUIRE(draft != nullptr);
    REQUIRE(findOperation(*draft, OperationType::Download) != nullptr);

    bool hasBlockedWarning = false;
    for (const QString& warning : draft->warnings)
    {
        if (warning.contains(QStringLiteral("type mismatch")))
        {
            hasBlockedWarning = true;
        }
    }
    CHECK(hasBlockedWarning);
}

TEST_CASE("Uniform directory transfer covers descendants and aggregates consequences")
{
    FakeTreeBuilder localBuilder;
    localBuilder.addFile(QStringLiteral("newdir/a.txt"), 10, 100, "a");
    localBuilder.addFile(QStringLiteral("newdir/b.txt"), 20, 100, "b");
    const Classification classification = classify({localBuilder.build(), Tree()});

    const Planner planner;
    QHash<QString, Action> decisions;
    decisions.insert(QStringLiteral("newdir"), Action::LocalToRemote);
    const Plan plan = planner.plan(classification, decisions);

    const RowPlan* newdir = plan.find(QStringLiteral("newdir"));
    REQUIRE(newdir != nullptr);
    REQUIRE(newdir->operations.size() == 1);
    CHECK(newdir->operations.first().type == OperationType::Upload);
    CHECK(newdir->operations.first().isSubtree);
    CHECK(newdir->operations.first().isFolder);

    CHECK(containsPath(newdir->createdRemote, QStringLiteral("newdir")));
    CHECK(containsPath(newdir->createdRemote, QStringLiteral("newdir/a.txt")));
    CHECK(containsPath(newdir->createdRemote, QStringLiteral("newdir/b.txt")));

    const RowPlan* a = plan.find(QStringLiteral("newdir/a.txt"));
    REQUIRE(a != nullptr);
    CHECK(a->coveredByPath == QStringLiteral("newdir"));
    CHECK(a->operations.isEmpty());
    CHECK(a->action == Action::LocalToRemote);
    CHECK(a->actionSource == ActionSource::InheritedFromDirectory);
    CHECK(a->decisionSourcePath == QStringLiteral("newdir"));
}

TEST_CASE("Explicit child decision overrides the directory cascade")
{
    FakeTreeBuilder localBuilder;
    localBuilder.addFile(QStringLiteral("newdir/a.txt"), 10, 100, "a");
    localBuilder.addFile(QStringLiteral("newdir/b.txt"), 20, 100, "b");
    const Classification classification = classify({localBuilder.build(), Tree()});

    const Planner planner;
    QHash<QString, Action> decisions;
    decisions.insert(QStringLiteral("newdir"), Action::LocalToRemote);
    decisions.insert(QStringLiteral("newdir/a.txt"), Action::RemoteToLocal);  // trash locally
    const Plan plan = planner.plan(classification, decisions);

    const RowPlan* newdir = plan.find(QStringLiteral("newdir"));
    REQUIRE(newdir != nullptr);
    REQUIRE(newdir->operations.size() == 1);
    // Mixed decisions: node-only upload (folder creation), no subtree op.
    CHECK(newdir->operations.first().type == OperationType::Upload);
    CHECK_FALSE(newdir->operations.first().isSubtree);

    const RowPlan* a = plan.find(QStringLiteral("newdir/a.txt"));
    REQUIRE(a != nullptr);
    CHECK(a->coveredByPath.isEmpty());
    REQUIRE(a->operations.size() == 1);
    CHECK(a->operations.first().type == OperationType::DeleteLocalToTrash);
    CHECK(a->actionSource == ActionSource::OwnDecision);

    const RowPlan* b = plan.find(QStringLiteral("newdir/b.txt"));
    REQUIRE(b != nullptr);
    // Directory is node-only, so b plans its own upload.
    REQUIRE(b->operations.size() == 1);
    CHECK(b->operations.first().type == OperationType::Upload);

    // Aggregated consequences.
    CHECK(containsPath(newdir->createdRemote, QStringLiteral("newdir")));
    CHECK(containsPath(newdir->createdRemote, QStringLiteral("newdir/b.txt")));
    CHECK(containsPath(newdir->removedLocal, QStringLiteral("newdir/a.txt")));
}

TEST_CASE("Uniform directory deletion removes the whole subtree to Rubbish")
{
    FakeTreeBuilder remoteBuilder;
    remoteBuilder.addFile(QStringLiteral("olddir/x.txt"), 10, 100, "x");
    const Classification classification = classify({Tree(), remoteBuilder.build()});

    const Planner planner;
    QHash<QString, Action> decisions;
    decisions.insert(QStringLiteral("olddir"), Action::LocalToRemote);
    const Plan plan = planner.plan(classification, decisions);

    const RowPlan* olddir = plan.find(QStringLiteral("olddir"));
    REQUIRE(olddir != nullptr);
    REQUIRE(olddir->operations.size() == 1);
    CHECK(olddir->operations.first().type == OperationType::DeleteRemoteToRubbish);
    CHECK(olddir->operations.first().isSubtree);

    CHECK(containsPath(olddir->removedRemote, QStringLiteral("olddir")));
    CHECK(containsPath(olddir->removedRemote, QStringLiteral("olddir/x.txt")));

    const RowPlan* x = plan.find(QStringLiteral("olddir/x.txt"));
    REQUIRE(x != nullptr);
    CHECK(x->coveredByPath == QStringLiteral("olddir"));
    CHECK(x->operations.isEmpty());
}

TEST_CASE("Subtree deletion keeps explicit keeper decisions visible with a warning")
{
    FakeTreeBuilder remoteBuilder;
    remoteBuilder.addFile(QStringLiteral("olddir/x.txt"), 10, 100, "x");
    const Classification classification = classify({Tree(), remoteBuilder.build()});

    const Planner planner;
    QHash<QString, Action> decisions;
    decisions.insert(QStringLiteral("olddir"), Action::LocalToRemote);
    decisions.insert(QStringLiteral("olddir/x.txt"), Action::RemoteToLocal);  // download keeper
    const Plan plan = planner.plan(classification, decisions);

    const RowPlan* x = plan.find(QStringLiteral("olddir/x.txt"));
    REQUIRE(x != nullptr);
    CHECK(x->coveredByPath.isEmpty());
    REQUIRE(x->operations.size() == 1);
    CHECK(x->operations.first().type == OperationType::Download);

    const RowPlan* olddir = plan.find(QStringLiteral("olddir"));
    REQUIRE(olddir != nullptr);
    // The deletion still takes the file structurally; the warning says so.
    CHECK(containsPath(olddir->removedRemote, QStringLiteral("olddir/x.txt")));
    CHECK(containsPath(olddir->createdLocal, QStringLiteral("olddir/x.txt")));
    CHECK_FALSE(olddir->warnings.isEmpty());
}

TEST_CASE("Explicit matching child deletion is deduplicated under a subtree deletion")
{
    FakeTreeBuilder remoteBuilder;
    remoteBuilder.addFile(QStringLiteral("olddir/x.txt"), 10, 100, "x");
    const Classification classification = classify({Tree(), remoteBuilder.build()});

    const Planner planner;
    QHash<QString, Action> decisions;
    decisions.insert(QStringLiteral("olddir"), Action::LocalToRemote);
    decisions.insert(QStringLiteral("olddir/x.txt"), Action::LocalToRemote);  // matching deletion
    const Plan plan = planner.plan(classification, decisions);

    const RowPlan* x = plan.find(QStringLiteral("olddir/x.txt"));
    REQUIRE(x != nullptr);
    CHECK(x->coveredByPath == QStringLiteral("olddir"));
    CHECK(x->operations.isEmpty());

    const RowPlan* olddir = plan.find(QStringLiteral("olddir"));
    REQUIRE(olddir != nullptr);
    CHECK(containsPath(olddir->removedRemote, QStringLiteral("olddir/x.txt")));
    CHECK(olddir->warnings.isEmpty());
}

TEST_CASE("Nested uniform directories aggregate consequences at the root decision")
{
    FakeTreeBuilder localBuilder;
    localBuilder.addFile(QStringLiteral("p/q/f.txt"), 10, 100, "f");
    localBuilder.addFile(QStringLiteral("p/g.txt"), 20, 100, "g");
    const Classification classification = classify({localBuilder.build(), Tree()});

    const Planner planner;
    QHash<QString, Action> decisions;
    decisions.insert(QStringLiteral("p"), Action::LocalToRemote);
    const Plan plan = planner.plan(classification, decisions);

    const RowPlan* p = plan.find(QStringLiteral("p"));
    REQUIRE(p != nullptr);
    REQUIRE(p->operations.size() == 1);
    CHECK(p->operations.first().isSubtree);

    CHECK(containsPath(p->createdRemote, QStringLiteral("p")));
    CHECK(containsPath(p->createdRemote, QStringLiteral("p/q/f.txt")));
    CHECK(containsPath(p->createdRemote, QStringLiteral("p/g.txt")));

    const RowPlan* q = plan.find(QStringLiteral("p/q"));
    REQUIRE(q != nullptr);
    CHECK(q->coveredByPath == QStringLiteral("p"));
    CHECK(q->operations.isEmpty());

    const RowPlan* f = plan.find(QStringLiteral("p/q/f.txt"));
    REQUIRE(f != nullptr);
    CHECK(f->coveredByPath == QStringLiteral("p"));
}

TEST_CASE("Conflict twins resolve by rename-aware adoption on the arrows")
{
    FakeTreeBuilder localBuilder;
    FakeTreeBuilder remoteBuilder;
    localBuilder.addFile(QStringLiteral("a.txt"), 50, 500, "twin");
    remoteBuilder.addFile(QStringLiteral("b.txt"), 50, 500, "twin");
    const Classification classification = classify({localBuilder.build(), remoteBuilder.build()});

    const Planner planner;
    QHash<QString, Action> decisions;
    decisions.insert(QStringLiteral("a.txt"), Action::LocalToRemote);
    decisions.insert(QStringLiteral("b.txt"), Action::RemoteToLocal);
    const Plan plan = planner.plan(classification, decisions);

    // The local row's arrow adopts the identical remote twin by renaming
    // it to a.txt: no duplicate upload, no bytes transferred (MEGA-2.9).
    const RowPlan* localTwin = plan.find(QStringLiteral("a.txt"));
    REQUIRE(localTwin != nullptr);
    REQUIRE(localTwin->operations.size() == 1);
    CHECK(localTwin->operations.first().type == OperationType::RenameRemote);
    CHECK(localTwin->operations.first().fromPath == QStringLiteral("b.txt"));
    CHECK(localTwin->operations.first().toPath == QStringLiteral("a.txt"));
    CHECK(findOperation(*localTwin, OperationType::Upload) == nullptr);
    CHECK_FALSE(localTwin->warnings.isEmpty());
    REQUIRE(localTwin->renamedRemote.size() == 1);
    CHECK(localTwin->renamedRemote.first().first == QStringLiteral("b.txt"));

    // The adopt consumes the twin row's entry, so the remote-side row is
    // covered by the adopt and its own decision is superseded.
    const RowPlan* remoteTwin = plan.find(QStringLiteral("b.txt"));
    REQUIRE(remoteTwin != nullptr);
    CHECK(remoteTwin->coveredByPath == QStringLiteral("a.txt"));
    CHECK(remoteTwin->operations.isEmpty());
    CHECK_FALSE(remoteTwin->warnings.isEmpty());

    // Best-effort keeps the plain transfer and its duplicate warning.
    decisions.clear();
    decisions.insert(QStringLiteral("a.txt"), Action::BestEffort);
    const Plan bestEffortPlan = planner.plan(classification, decisions);
    const RowPlan* bestEffort = bestEffortPlan.find(QStringLiteral("a.txt"));
    REQUIRE(bestEffort != nullptr);
    REQUIRE(findOperation(*bestEffort, OperationType::Upload) != nullptr);
    CHECK(bestEffort->warnings.join(QLatin1Char(' ')).contains(QStringLiteral("b.txt")));
}

TEST_CASE("Directory transfer over a conflict twin warns about the duplicate")
{
    const Classification classification = classify(FakeScenarios::edgeCaseKitchenSink());
    const Planner planner;

    QHash<QString, Action> decisions;
    decisions.insert(QStringLiteral("archive"), Action::LocalToRemote);
    const Plan plan = planner.plan(classification, decisions);

    const RowPlan* archive = plan.find(QStringLiteral("archive"));
    REQUIRE(archive != nullptr);
    REQUIRE(archive->operations.size() == 1);
    CHECK(archive->operations.first().type == OperationType::Upload);
    CHECK(archive->warnings.join(QLatin1Char(' ')).contains(QStringLiteral("old/b.txt")));
}

TEST_CASE("Explicit none on a directory cascades to descendants without decisions")
{
    const Classification classification = classify(FakeScenarios::edgeCaseKitchenSink());
    const Planner planner;

    QHash<QString, Action> decisions;
    decisions.insert(QStringLiteral("photos"), Action::None);
    const Plan plan = planner.plan(classification, decisions);

    const RowPlan* img1 = plan.find(QStringLiteral("photos/img1.png"));
    REQUIRE(img1 != nullptr);
    CHECK(img1->action == Action::None);
    CHECK(img1->actionSource == ActionSource::InheritedFromDirectory);
    CHECK(img1->operations.isEmpty());

    const RowPlan* img2 = plan.find(QStringLiteral("photos/img2.png"));
    REQUIRE(img2 != nullptr);
    CHECK(img2->operations.isEmpty());
}

TEST_CASE("Recommended actions plan the whole kitchen sink without decisions")
{
    const Classification classification = classify(FakeScenarios::edgeCaseKitchenSink());
    const Planner planner;
    const Plan plan = planner.plan(classification, {});

    REQUIRE(plan.rows.size() == classification.rows.size());

    // Identical rows: nothing planned.
    const RowPlan* docs = plan.find(QStringLiteral("docs"));
    REQUIRE(docs != nullptr);
    CHECK(docs->operations.isEmpty());

    // Single-sided recommendations transfer content.
    const RowPlan* onlyLocal = plan.find(QStringLiteral("only_local.txt"));
    REQUIRE(onlyLocal != nullptr);
    CHECK(findOperation(*onlyLocal, OperationType::Upload) != nullptr);

    const RowPlan* onlyRemote = plan.find(QStringLiteral("only_remote.txt"));
    REQUIRE(onlyRemote != nullptr);
    CHECK(findOperation(*onlyRemote, OperationType::Download) != nullptr);

    // Newer remote side downloads (replace).
    const RowPlan* img1 = plan.find(QStringLiteral("photos/img1.png"));
    REQUIRE(img1 != nullptr);
    CHECK(findOperation(*img1, OperationType::DownloadReplace) != nullptr);

    // Tie recommends L->R (replace upload).
    const RowPlan* img2 = plan.find(QStringLiteral("photos/img2.png"));
    REQUIRE(img2 != nullptr);
    CHECK(findOperation(*img2, OperationType::UploadReplace) != nullptr);

    // Conflict rows: no default action, nothing planned.
    const RowPlan* localTwin = plan.find(QStringLiteral("archive/a.txt"));
    REQUIRE(localTwin != nullptr);
    CHECK(localTwin->operations.isEmpty());
}

TEST_CASE("Empty classification produces an empty plan")
{
    const Classification classification = classify(FakeScenarios::emptySides());
    const Planner planner;
    const Plan plan = planner.plan(classification, {});

    CHECK(plan.rows.isEmpty());
    CHECK(plan.find(QStringLiteral("anything")) == nullptr);
}

TEST_CASE("Rename-swap row resolves L->R with the rename-aware exchange")
{
    const Classification classification = classify(FakeScenarios::renameSwap());
    const Planner planner;

    // Pass D flags both the paired row and the leftover with the advisory
    // twin; the leftover stays a plain RemoteOnly row (no approval).
    const Row* fooRow = classification.find(QStringLiteral("foo.txt"));
    REQUIRE(fooRow != nullptr);
    CHECK(fooRow->kind == RowKind::BothDiffer);
    CHECK(fooRow->hasIdenticalTwin);
    CHECK(fooRow->twinPath == QStringLiteral("bar.txt"));
    const Row* barRow = classification.find(QStringLiteral("bar.txt"));
    REQUIRE(barRow != nullptr);
    CHECK(barRow->kind == RowKind::RemoteOnly);
    CHECK(barRow->hasIdenticalTwin);
    CHECK(barRow->twinPath == QStringLiteral("foo.txt"));
    CHECK_FALSE(barRow->requiresApproval);

    QHash<QString, Action> decisions;
    decisions.insert(QStringLiteral("foo.txt"), Action::LocalToRemote);
    const Plan swapPlan = planner.plan(classification, decisions);
    const RowPlan* foo = swapPlan.find(QStringLiteral("foo.txt"));
    REQUIRE(foo != nullptr);

    // Displacement first (the remote edit moves into the twin's vacated
    // name), then the adoption at the canonical path: a rename-aware swap,
    // no UploadReplace that would destroy content.
    REQUIRE(foo->operations.size() == 2);
    CHECK(foo->operations.first().type == OperationType::RenameRemote);
    CHECK(foo->operations.first().fromPath == QStringLiteral("foo.txt"));
    CHECK(foo->operations.first().toPath == QStringLiteral("bar.txt"));
    CHECK(foo->operations.last().type == OperationType::RenameRemote);
    CHECK(foo->operations.last().fromPath == QStringLiteral("bar.txt"));
    CHECK(foo->operations.last().toPath == QStringLiteral("foo.txt"));
    CHECK(findOperation(*foo, OperationType::UploadReplace) == nullptr);
    REQUIRE(foo->renamedRemote.size() == 2);
    CHECK_FALSE(foo->warnings.isEmpty());

    // The leftover row's entry moves with the swap: it is covered.
    const RowPlan* bar = swapPlan.find(QStringLiteral("bar.txt"));
    REQUIRE(bar != nullptr);
    CHECK(bar->coveredByPath == QStringLiteral("foo.txt"));
    CHECK(bar->operations.isEmpty());

    // R->L has no local counterpart to adopt: the plain replace stands.
    decisions.clear();
    decisions.insert(QStringLiteral("foo.txt"), Action::RemoteToLocal);
    const Plan reversePlan = planner.plan(classification, decisions);
    const RowPlan* reverse = reversePlan.find(QStringLiteral("foo.txt"));
    REQUIRE(reverse != nullptr);
    REQUIRE(reverse->operations.size() == 1);
    CHECK(reverse->operations.first().type == OperationType::DownloadReplace);
}

TEST_CASE("Double rename arms both adopt directions on the paired row")
{
    const Classification classification = classify(FakeScenarios::renameChain());

    const Row* foo = classification.find(QStringLiteral("foo.txt"));
    REQUIRE(foo != nullptr);
    CHECK(foo->twinPath == QStringLiteral("bar.txt"));
    CHECK(foo->localTwinPath == QStringLiteral("foo2.txt"));

    const Planner planner;
    QHash<QString, Action> decisions;
    decisions.insert(QStringLiteral("foo.txt"), Action::RemoteToLocal);
    const Plan reversePlan = planner.plan(classification, decisions);
    const RowPlan* reverse = reversePlan.find(QStringLiteral("foo.txt"));
    REQUIRE(reverse != nullptr);

    // R->L adopts the local twin: the displaced local entry (whose content
    // is unique locally) moves into the vacated twin name, then the twin
    // content lands at foo.txt.
    REQUIRE(reverse->operations.size() == 2);
    CHECK(reverse->operations.first().type == OperationType::RenameLocal);
    CHECK(reverse->operations.first().fromPath == QStringLiteral("foo.txt"));
    CHECK(reverse->operations.first().toPath == QStringLiteral("foo2.txt"));
    CHECK(reverse->operations.last().type == OperationType::RenameLocal);
    CHECK(reverse->operations.last().fromPath == QStringLiteral("foo2.txt"));
    CHECK(reverse->operations.last().toPath == QStringLiteral("foo.txt"));
    CHECK(findOperation(*reverse, OperationType::DownloadReplace) == nullptr);
}

TEST_CASE("Rename plus edit with no counterpart plans the plain replace with an advisory")
{
    const Classification classification = classify(FakeScenarios::renameEdit());
    const Planner planner;

    // Three distinct contents, no identical counterpart anywhere: no twin
    // flags, so no rename-aware resolution is offered.
    const Row* foo = classification.find(QStringLiteral("foo.txt"));
    REQUIRE(foo != nullptr);
    CHECK_FALSE(foo->hasIdenticalTwin);
    CHECK(foo->twinPath.isEmpty());
    CHECK(foo->localTwinPath.isEmpty());
    const Row* bar = classification.find(QStringLiteral("bar.txt"));
    REQUIRE(bar != nullptr);
    CHECK(bar->kind == RowKind::RemoteOnly);
    CHECK_FALSE(bar->hasIdenticalTwin);

    QHash<QString, Action> decisions;
    decisions.insert(QStringLiteral("foo.txt"), Action::LocalToRemote);
    const Plan fooPlan = planner.plan(classification, decisions);
    const RowPlan* fooPlanRow = fooPlan.find(QStringLiteral("foo.txt"));
    REQUIRE(fooPlanRow != nullptr);
    REQUIRE(fooPlanRow->operations.size() == 1);
    CHECK(fooPlanRow->operations.first().type == OperationType::UploadReplace);
    // The replaced remote content is the only copy on the remote side.
    CHECK(fooPlanRow->warnings.join(QLatin1Char(' ')).contains(QStringLiteral("not preserved")));
}

TEST_CASE("Case-collision blocker decides with displacement plus transfer")
{
    const Classification classification = classify(FakeScenarios::edgeCaseKitchenSink());
    const Planner planner;

    QHash<QString, Action> decisions;
    decisions.insert(QStringLiteral("notes/A.txt"), Action::LocalToRemote);
    const Plan uploadPlan = planner.plan(classification, decisions);
    const RowPlan* upload = uploadPlan.find(QStringLiteral("notes/A.txt"));
    REQUIRE(upload != nullptr);

    // L->R displaces the colliding remote spelling (first free
    // case-insensitive name), then uploads the local content.
    REQUIRE(upload->operations.size() == 2);
    CHECK(upload->operations.first().type == OperationType::RenameRemote);
    CHECK(upload->operations.first().fromPath == QStringLiteral("notes/a.txt"));
    CHECK(upload->operations.first().toPath == QStringLiteral("notes/a (1).txt"));
    CHECK(upload->operations.last().type == OperationType::Upload);
    CHECK(upload->operations.last().path == QStringLiteral("notes/A.txt"));
    REQUIRE(upload->renamedRemote.size() == 1);
    CHECK(containsPath(upload->createdRemote, QStringLiteral("notes/A.txt")));
    CHECK_FALSE(upload->warnings.isEmpty());

    decisions.clear();
    decisions.insert(QStringLiteral("notes/A.txt"), Action::RemoteToLocal);
    const Plan downloadPlan = planner.plan(classification, decisions);
    const RowPlan* download = downloadPlan.find(QStringLiteral("notes/A.txt"));
    REQUIRE(download != nullptr);

    // R->L displaces the local spelling and downloads the remote content
    // under the remote's own spelling.
    REQUIRE(download->operations.size() == 2);
    CHECK(download->operations.first().type == OperationType::RenameLocal);
    CHECK(download->operations.first().fromPath == QStringLiteral("notes/A.txt"));
    CHECK(download->operations.first().toPath == QStringLiteral("notes/A (1).txt"));
    CHECK(download->operations.last().type == OperationType::Download);
    CHECK(download->operations.last().path == QStringLiteral("notes/a.txt"));

    // Best-effort cannot resolve a blocked row.
    decisions.clear();
    decisions.insert(QStringLiteral("notes/A.txt"), Action::BestEffort);
    const Plan mergePlan = planner.plan(classification, decisions);
    const RowPlan* merge = mergePlan.find(QStringLiteral("notes/A.txt"));
    REQUIRE(merge != nullptr);
    CHECK(merge->operations.isEmpty());
    CHECK_FALSE(merge->warnings.isEmpty());
}

TEST_CASE("Type-mismatch blocker displaces by rename; folder names keep their whole name")
{
    const Classification classification = classify(FakeScenarios::edgeCaseKitchenSink());
    const Planner planner;

    QHash<QString, Action> decisions;
    decisions.insert(QStringLiteral("misc/notes.txt"), Action::LocalToRemote);
    const Plan uploadPlan = planner.plan(classification, decisions);
    const RowPlan* upload = uploadPlan.find(QStringLiteral("misc/notes.txt"));
    REQUIRE(upload != nullptr);

    // L->R renames the remote FOLDER aside (whole name, no extension
    // split) and uploads the local file at the freed path.
    REQUIRE(upload->operations.size() == 2);
    CHECK(upload->operations.first().type == OperationType::RenameRemote);
    CHECK(upload->operations.first().fromPath == QStringLiteral("misc/notes.txt"));
    CHECK(upload->operations.first().toPath == QStringLiteral("misc/notes.txt (1)"));
    CHECK(upload->operations.first().isFolder);
    CHECK(upload->operations.last().type == OperationType::Upload);
    CHECK(upload->operations.last().path == QStringLiteral("misc/notes.txt"));
    CHECK_FALSE(upload->operations.last().isFolder);

    decisions.clear();
    decisions.insert(QStringLiteral("misc/notes.txt"), Action::RemoteToLocal);
    const Plan downloadPlan = planner.plan(classification, decisions);
    const RowPlan* download = downloadPlan.find(QStringLiteral("misc/notes.txt"));
    REQUIRE(download != nullptr);

    // R->L renames the local FILE aside (extension kept) and downloads the
    // remote folder at the freed path.
    REQUIRE(download->operations.size() == 2);
    CHECK(download->operations.first().type == OperationType::RenameLocal);
    CHECK(download->operations.first().fromPath == QStringLiteral("misc/notes.txt"));
    CHECK(download->operations.first().toPath == QStringLiteral("misc/notes (1).txt"));
    CHECK_FALSE(download->operations.first().isFolder);
    CHECK(download->operations.last().type == OperationType::Download);
    CHECK(download->operations.last().path == QStringLiteral("misc/notes.txt"));
    CHECK(download->operations.last().isFolder);
}

TEST_CASE("Kitchen-sink rename twin arms the paired row's L->R adopt")
{
    const Classification classification = classify(FakeScenarios::edgeCaseKitchenSink());
    const Planner planner;

    const Row* main = classification.find(QStringLiteral("projects/mega/client/src/main.cpp"));
    REQUIRE(main != nullptr);
    CHECK(main->kind == RowKind::BothDiffer);
    CHECK(main->hasIdenticalTwin);
    CHECK(main->twinPath == QStringLiteral("projects/mega/client/backup/main.cpp"));

    // The recommended action (remote newer -> R->L) stays a plain replace;
    // the adopt arms only the L->R direction.
    const Plan recommended = planner.plan(classification, {});
    const RowPlan* recommendedMain = recommended.find(QStringLiteral("projects/mega/client/src/main.cpp"));
    REQUIRE(recommendedMain != nullptr);
    REQUIRE(recommendedMain->operations.size() == 1);
    CHECK(recommendedMain->operations.first().type == OperationType::DownloadReplace);

    QHash<QString, Action> decisions;
    decisions.insert(QStringLiteral("projects/mega/client/src/main.cpp"), Action::LocalToRemote);
    const Plan adoptPlan = planner.plan(classification, decisions);
    const RowPlan* adopted = adoptPlan.find(QStringLiteral("projects/mega/client/src/main.cpp"));
    REQUIRE(adopted != nullptr);
    REQUIRE(adopted->operations.size() == 2);
    CHECK(adopted->operations.first().type == OperationType::RenameRemote);
    CHECK(adopted->operations.first().fromPath == QStringLiteral("projects/mega/client/src/main.cpp"));
    CHECK(adopted->operations.first().toPath == QStringLiteral("projects/mega/client/backup/main.cpp"));
    CHECK(adopted->operations.last().fromPath == QStringLiteral("projects/mega/client/backup/main.cpp"));
    CHECK(adopted->operations.last().toPath == QStringLiteral("projects/mega/client/src/main.cpp"));
}

TEST_CASE("A leftover twin claimed once serves one paired row")
{
    FakeTreeBuilder localBuilder;
    FakeTreeBuilder remoteBuilder;
    // Two paired rows differ on both sides; their shared local content
    // exists on the remote side only once (the leftover) — it can serve
    // one paired row's adopt.
    localBuilder.addFile(QStringLiteral("one.txt"), 10, 100, "shared");
    localBuilder.addFile(QStringLiteral("two.txt"), 10, 100, "shared");
    remoteBuilder.addFile(QStringLiteral("one.txt"), 10, 100, "one-remote");
    remoteBuilder.addFile(QStringLiteral("two.txt"), 10, 100, "two-remote");
    remoteBuilder.addFile(QStringLiteral("spare.txt"), 10, 100, "shared");
    const Classification classification = classify({localBuilder.build(), remoteBuilder.build()});

    const Row* one = classification.find(QStringLiteral("one.txt"));
    REQUIRE(one != nullptr);
    CHECK(one->hasIdenticalTwin);
    CHECK(one->twinPath == QStringLiteral("spare.txt"));
    const Row* two = classification.find(QStringLiteral("two.txt"));
    REQUIRE(two != nullptr);
    CHECK_FALSE(two->hasIdenticalTwin);
    CHECK(two->twinPath.isEmpty());
}
