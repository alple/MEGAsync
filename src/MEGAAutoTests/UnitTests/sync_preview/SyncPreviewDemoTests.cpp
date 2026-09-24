#include "FakeSyncPreviewProvider.h"
#include "SyncPreviewClassifier.h"
#include "SyncPreviewPlanner.h"

#include <QHash>
#include <QTextStream>

#include <catch.hpp>

using namespace SyncPreview;

// Not a verification test: a human-readable dump of what the sync_preview
// core produces on the fake kitchen-sink pair. Run with:
//   just demo-syncpreview
// (prints via Catch2's -s success output).
namespace
{
    Classification classify(const FakeScenario& scenario)
    {
        const Classifier classifier;
        const FakeSideProvider localProvider(scenario.local);
        const FakeSideProvider remoteProvider(scenario.remote);
        return classifier.classify(localProvider, remoteProvider);
    }

    QString kindName(RowKind kind)
    {
        switch (kind)
        {
            case RowKind::LocalOnly:
                return QStringLiteral("local-only");
            case RowKind::RemoteOnly:
                return QStringLiteral("remote-only");
            case RowKind::Identical:
                return QStringLiteral("identical");
            case RowKind::BothDiffer:
                return QStringLiteral("both-differ");
            case RowKind::Conflict:
                return QStringLiteral("conflict");
            case RowKind::Blocker:
                return QStringLiteral("blocker");
        }
        return QStringLiteral("?");
    }

    QString actionName(Action action)
    {
        switch (action)
        {
            case Action::LocalToRemote:
                return QStringLiteral("L->R");
            case Action::RemoteToLocal:
                return QStringLiteral("R->L");
            case Action::BestEffort:
                return QStringLiteral("best-effort");
            case Action::None:
                return QStringLiteral("none");
        }
        return QStringLiteral("?");
    }

    QString operationName(const PlannedOperation& operation)
    {
        QString name;
        switch (operation.type)
        {
            case OperationType::Upload:
                name = QStringLiteral("upload");
                break;
            case OperationType::Download:
                name = QStringLiteral("download");
                break;
            case OperationType::UploadReplace:
                name = QStringLiteral("upload-replace (old -> Rubbish)");
                break;
            case OperationType::DownloadReplace:
                name = QStringLiteral("download-replace (old -> trash)");
                break;
            case OperationType::DeleteRemoteToRubbish:
                name = QStringLiteral("delete-remote (-> Rubbish)");
                break;
            case OperationType::DeleteLocalToTrash:
                name = QStringLiteral("delete-local (-> trash)");
                break;
            case OperationType::None:
                return QStringLiteral("-");
        }
        if (operation.isFolder)
        {
            name += QStringLiteral(" [folder]");
        }
        if (operation.isSubtree)
        {
            name += QStringLiteral(" [subtree]");
        }
        return name;
    }

    QString sidesOf(const Row& row)
    {
        QString localSide;
        QString remoteSide;
        if (row.local)
        {
            localSide = row.local->isFolder() ? QStringLiteral("dir") : QStringLiteral("file");
        }
        if (row.remote)
        {
            remoteSide = row.remote->isFolder() ? QStringLiteral("dir") : QStringLiteral("file");
        }
        if (!localSide.isEmpty() && !remoteSide.isEmpty())
        {
            return QStringLiteral("L:%1 R:%2").arg(localSide, remoteSide);
        }
        if (!localSide.isEmpty())
        {
            return QStringLiteral("L:%1").arg(localSide);
        }
        return QStringLiteral("R:%1").arg(remoteSide);
    }

    void printClassification(QTextStream& out, const Classification& classification)
    {
        out << QString(QStringLiteral("-- classification: %1 rows --\n")).arg(classification.rows.size());
        out << QStringLiteral("%1 | %2 | %3 | %4 | %5\n")
                   .arg(QStringLiteral("path").leftJustified(28),
                        QStringLiteral("kind").leftJustified(12),
                        QStringLiteral("appr").leftJustified(4),
                        QStringLiteral("rec").leftJustified(10),
                        QStringLiteral("sides"));
        for (const Row& row : classification.rows)
        {
            QString flags;
            if (row.requiresApproval)
            {
                flags += QStringLiteral("Y");
            }
            if (row.hasIdenticalTwin)
            {
                flags += QStringLiteral(",twin->%1").arg(row.twinPath);
            }
            if (row.underBlockedPath)
            {
                flags += QStringLiteral(",blocked-parent");
            }
            out << QStringLiteral("%1 | %2 | %3 | %4 | %5\n")
                       .arg(row.relativePath.leftJustified(28),
                            kindName(row.kind).leftJustified(12),
                            flags.leftJustified(4),
                            actionName(row.recommendedAction).leftJustified(10),
                            sidesOf(row));
        }
    }

    void printPlan(QTextStream& out, const QString& title, const Plan& plan)
    {
        out << QStringLiteral("\n-- plan: %1 --\n").arg(title);
        out << QStringLiteral("%1 | %2 | %3 | %4\n")
                   .arg(QStringLiteral("path").leftJustified(28),
                        QStringLiteral("action").leftJustified(12),
                        QStringLiteral("src").leftJustified(10),
                        QStringLiteral("operations"));
        for (const RowPlan& rowPlan : plan.rows)
        {
            QStringList operationNames;
            for (const PlannedOperation& operation : rowPlan.operations)
            {
                operationNames.append(operationName(operation));
            }
            QString source;
            if (rowPlan.actionSource == ActionSource::OwnDecision)
            {
                source = QStringLiteral("decided");
            }
            else if (rowPlan.actionSource == ActionSource::InheritedFromDirectory)
            {
                source = QStringLiteral("cascade");
            }
            else if (rowPlan.action == Action::None)
            {
                source = QStringLiteral("-");
            }
            else
            {
                source = QStringLiteral("recommended");
            }
            if (!rowPlan.coveredByPath.isEmpty())
            {
                source = QStringLiteral("covered by %1").arg(rowPlan.coveredByPath);
            }

            out << QStringLiteral("%1 | %2 | %3 | %4\n")
                       .arg(rowPlan.relativePath.leftJustified(28),
                            actionName(rowPlan.action).leftJustified(12),
                            source.leftJustified(10),
                            operationNames.isEmpty() ? QStringLiteral("-") : operationNames.join(QStringLiteral(", ")));
        }

        bool printedConsequencesHeader = false;
        for (const RowPlan& rowPlan : plan.rows)
        {
            const bool hasConsequences = !rowPlan.createdLocal.isEmpty() || !rowPlan.changedLocal.isEmpty() || !rowPlan.removedLocal.isEmpty()
                || !rowPlan.createdRemote.isEmpty() || !rowPlan.changedRemote.isEmpty() || !rowPlan.removedRemote.isEmpty();
            const bool hasWarnings = !rowPlan.warnings.isEmpty();
            if (!hasConsequences && !hasWarnings)
            {
                continue;
            }
            if (!printedConsequencesHeader)
            {
                out << QStringLiteral("\n   consequences / warnings:\n");
                printedConsequencesHeader = true;
            }
            out << QStringLiteral("   %1:\n").arg(rowPlan.relativePath);
            if (!rowPlan.createdLocal.isEmpty())
            {
                out << QStringLiteral("      created local:    %1\n").arg(rowPlan.createdLocal.join(QStringLiteral(", ")));
            }
            if (!rowPlan.changedLocal.isEmpty())
            {
                out << QStringLiteral("      changed local:    %1\n").arg(rowPlan.changedLocal.join(QStringLiteral(", ")));
            }
            if (!rowPlan.removedLocal.isEmpty())
            {
                out << QStringLiteral("      removed local:    %1 (-> trash)\n").arg(rowPlan.removedLocal.join(QStringLiteral(", ")));
            }
            if (!rowPlan.createdRemote.isEmpty())
            {
                out << QStringLiteral("      created remote:   %1\n").arg(rowPlan.createdRemote.join(QStringLiteral(", ")));
            }
            if (!rowPlan.changedRemote.isEmpty())
            {
                out << QStringLiteral("      changed remote:   %1\n").arg(rowPlan.changedRemote.join(QStringLiteral(", ")));
            }
            if (!rowPlan.removedRemote.isEmpty())
            {
                out << QStringLiteral("      removed remote:   %1 (-> Rubbish)\n").arg(rowPlan.removedRemote.join(QStringLiteral(", ")));
            }
            for (const QString& warning : rowPlan.warnings)
            {
                out << QStringLiteral("      WARNING: %1\n").arg(warning);
            }
        }
    }
}

TEST_CASE("sync preview demo: kitchen-sink classification and plans")
{
    const Classification classification = classify(FakeScenarios::edgeCaseKitchenSink());
    CHECK_FALSE(classification.rows.isEmpty());  // dump guard; the value is the printed output

    QTextStream out(stdout);
    out << QStringLiteral("\n=== sync preview demo — kitchen-sink pair (fake data) ===\n\n");
    printClassification(out, classification);

    const Planner planner;
    printPlan(out, QStringLiteral("recommended actions only (no explicit decisions)"), planner.plan(classification, {}));

    QHash<QString, Action> decisions;
    decisions.insert(QStringLiteral("archive"), Action::LocalToRemote);          // dir cascade over a conflict twin
    decisions.insert(QStringLiteral("old/b.txt"), Action::RemoteToLocal);        // the twin's other side, decided
    decisions.insert(QStringLiteral("photos/img2.png"), Action::RemoteToLocal);  // override the tie recommendation
    printPlan(out, QStringLiteral("sample decisions: archive->L->R, old/b.txt->R->L, photos/img2.png->R->L"), planner.plan(classification, decisions));

    out.flush();
}
