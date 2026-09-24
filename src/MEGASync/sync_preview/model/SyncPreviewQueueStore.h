#ifndef SYNCPREVIEWQUEUESTORE_H
#define SYNCPREVIEWQUEUESTORE_H

#include "SyncPreviewQueue.h"

#include <QByteArray>

namespace SyncPreview
{
    // Versioned JSON serialization of the pair queue and per-row decisions.
    // Stage 1 provided the in-memory codec; Stage 2 adds the file-backed
    // store (SyncPreviewQueueFileStore) and schema v2 (per-decision kind /
    // requiresApproval snapshots, per-pair completed flag). Deserialization
    // accepts schema v1 (fields absent → UNKNOWN_KIND snapshots, treated as
    // changed by the re-verify pass) and v2; any other version yields an
    // empty queue (all-or-nothing, per the MEGA-2 decision record).
    class QueueStore
    {
    public:
        static constexpr int SCHEMA_VERSION = 2;
        static constexpr int SCHEMA_VERSION_V1 = 1;

        static QByteArray serialize(const Queue& queue);

        // Restores a queue from serialized bytes. Any malformed input —
        // unparseable JSON, a missing or unsupported schema version, or any
        // structural violation inside — yields an empty queue (all-or-
        // nothing, per the MEGA-2 decision record on persistence).
        static Queue deserialize(const QByteArray& serialized);
    };
}

#endif // SYNCPREVIEWQUEUESTORE_H
