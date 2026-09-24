#ifndef SYNCPREVIEWQUEUEFILESTORE_H
#define SYNCPREVIEWQUEUEFILESTORE_H

#include "SyncPreviewQueue.h"

#include <QString>

namespace SyncPreview
{
    // File-backed persistence over the QueueStore codec: the fork-owned
    // sync-preview queue JSON. The file path is injected by the caller (the
    // app resolves it in the app config directory; tests pass a temp path).
    // Saves are atomic (QSaveFile): a crash mid-write leaves the previous
    // file intact. A missing or corrupted file restores an empty queue per
    // the MEGA-2 persistence decision record.
    class QueueFileStore
    {
    public:
        explicit QueueFileStore(QString filePath);

        // Missing or unreadable file → empty queue. Corrupted content →
        // empty queue (QueueStore::deserialize semantics).
        Queue load() const;

        // Serializes and atomically writes the queue. Returns false when the
        // write could not be committed (e.g. unwritable directory).
        bool save(const Queue& queue) const;

        const QString& filePath() const { return mFilePath; }

    private:
        QString mFilePath;
    };
}
#endif // SYNCPREVIEWQUEUEFILESTORE_H