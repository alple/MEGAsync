#ifndef SYNCPREVIEWRECONCILER_H
#define SYNCPREVIEWRECONCILER_H

#include "SyncPreviewClassifier.h"
#include "SyncPreviewQueue.h"

#include <QHash>
#include <QString>

namespace SyncPreview
{
    struct ReconcileResult
    {
        Queue queue;
        // Per pair id: relative paths whose classification changed since the
        // decision was snapshotted. Their approvals were cleared (re-flagged).
        QHash<QString, QStringList> reFlaggedPaths;
    };

    // Restore-time re-verify pass (MEGA-2 user model: "on reopen the queue is
    // restored and re-scanned; rows whose classification changed are
    // re-flagged; pairs drop off after their sync is created"). Pure function
    // of the restored queue and the fresh classifications.
    //
    // Per pair, per decision:
    // - the pair's row vanished → the decision is dropped;
    // - the row's kind or approval requirement changed (or the decision
    //   carries no snapshot, e.g. a schema v1 file) → the decision is kept,
    //   its approval is cleared, and the path is recorded as re-flagged;
    // - otherwise the decision is kept as-is. Snapshots are refreshed to the
    //   current classification in every surviving decision.
    // Pairs marked completed are dropped from the queue entirely. Pairs
    // without a fresh classification are kept unverified.
    class Reconciler
    {
    public:
        ReconcileResult reconcile(const Queue& restored,
                                  const QHash<QString, Classification>& fresh) const;
    };
}
#endif // SYNCPREVIEWRECONCILER_H