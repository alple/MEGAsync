#include "SyncPreviewReconciler.h"

namespace SyncPreview
{
    ReconcileResult Reconciler::reconcile(const Queue& restored,
                                          const QHash<QString, Classification>& fresh) const
    {
        ReconcileResult result;

        for (const Pair& restoredPair : restored.pairs)
        {
            if (restoredPair.completed)
            {
                continue;
            }

            Pair pair = restoredPair;
            const auto freshIt = fresh.constFind(pair.id);
            if (freshIt != fresh.constEnd())
            {
                const Classification& classification = freshIt.value();
                const Row* classificationRow = nullptr;

                QVector<RowDecision> reconciled;
                reconciled.reserve(pair.decisions.size());
                for (RowDecision decision : pair.decisions)
                {
                    classificationRow = classification.find(decision.relativePath);
                    if (!classificationRow)
                    {
                        // The row vanished from the re-scanned tree: the
                        // decision has nothing to apply to anymore.
                        continue;
                    }

                    const int currentKind = static_cast<int>(classificationRow->kind);
                    const bool changed =
                        decision.kind == RowDecision::UNKNOWN_KIND ||
                        decision.kind != currentKind ||
                        decision.requiresApproval != classificationRow->requiresApproval;
                    if (changed)
                    {
                        decision.approved = false;
                        result.reFlaggedPaths[pair.id].append(decision.relativePath);
                    }

                    decision.kind = currentKind;
                    decision.requiresApproval = classificationRow->requiresApproval;
                    reconciled.append(decision);
                }

                pair.decisions = reconciled;
            }

            result.queue.pairs.append(pair);
        }

        return result;
    }
}