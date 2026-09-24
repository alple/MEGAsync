#ifndef SYNCPREVIEWQUEUE_H
#define SYNCPREVIEWQUEUE_H

#include "SyncPreviewDefs.h"

#include <QString>
#include <QVector>

namespace SyncPreview
{
    // One persisted per-row decision with its approval state. Approval gates
    // the commit (Stage 5); the action is what the planner applies. The kind
    // and requiresApproval snapshots (schema v2) let the restore-time
    // re-verify pass detect rows whose classification changed since the
    // decision was made: those are re-flagged (approval cleared). Decisions
    // restored from schema v1 files carry UNKNOWN_KIND, which the re-verify
    // pass treats as changed.
    struct RowDecision
    {
        static constexpr int UNKNOWN_KIND = -1;

        QString relativePath;
        Action action = Action::None;
        bool approved = false;
        int kind = UNKNOWN_KIND;  // static_cast<int>(RowKind) when snapshotted
        bool requiresApproval = false;
    };

    // A local<->remote candidate pair queued for review. Tree snapshots are
    // deliberately not persisted: on reopen the queue is restored and
    // re-scanned, and rows whose classification changed are re-flagged.
    struct Pair
    {
        QString id;            // stable identifier
        QString localPath;     // local root folder path
        QString remotePath;    // remote root display path
        QString remoteHandle;  // remote root node handle
        // Set when the pair's sync was created (Stage 5); such pairs are
        // dropped from the queue on restore. The user can also remove pairs
        // by hand.
        bool completed = false;
        QVector<RowDecision> decisions;
    };

    struct Queue
    {
        QVector<Pair> pairs;
    };
}

#endif // SYNCPREVIEWQUEUE_H
