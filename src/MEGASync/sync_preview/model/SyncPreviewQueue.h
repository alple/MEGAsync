#ifndef SYNCPREVIEWQUEUE_H
#define SYNCPREVIEWQUEUE_H

#include "SyncPreviewDefs.h"

#include <QString>
#include <QVector>

namespace SyncPreview
{
    // One persisted per-row decision with its approval state. Approval gates
    // the commit (Stage 5); the action is what the planner applies.
    struct RowDecision
    {
        QString relativePath;
        Action action = Action::None;
        bool approved = false;
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
        QVector<RowDecision> decisions;
    };

    struct Queue
    {
        QVector<Pair> pairs;
    };
}

#endif // SYNCPREVIEWQUEUE_H
