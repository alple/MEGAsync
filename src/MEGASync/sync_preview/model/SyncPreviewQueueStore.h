#ifndef SYNCPREVIEWQUEUESTORE_H
#define SYNCPREVIEWQUEUESTORE_H

#include "SyncPreviewQueue.h"

#include <QByteArray>

namespace SyncPreview
{
    // Versioned JSON serialization of the pair queue and per-row decisions.
    // Stage 1 provides the in-memory codec; the fork-owned JSON file in the
    // app config directory is wired in a later stage (file I/O, atomic
    // writes).
    class QueueStore
    {
    public:
        static constexpr int SCHEMA_VERSION = 1;

        static QByteArray serialize(const Queue& queue);

        // Restores a queue from serialized bytes. Any malformed input —
        // unparseable JSON, a missing or unsupported schema version, or any
        // structural violation inside — yields an empty queue (all-or-
        // nothing, per the MEGA-2 decision record on persistence).
        static Queue deserialize(const QByteArray& serialized);
    };
}

#endif // SYNCPREVIEWQUEUESTORE_H
