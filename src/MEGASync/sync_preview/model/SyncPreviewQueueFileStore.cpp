#include "SyncPreviewQueueFileStore.h"
#include "SyncPreviewQueueStore.h"

#include <QSaveFile>
#include <QFile>

#include <utility>

namespace SyncPreview
{
    QueueFileStore::QueueFileStore(QString filePath) :
        mFilePath(std::move(filePath))
    {
    }

    Queue QueueFileStore::load() const
    {
        QFile file(mFilePath);
        if (!file.exists() || !file.open(QIODevice::ReadOnly))
        {
            return Queue();
        }

        return QueueStore::deserialize(file.readAll());
    }

    bool QueueFileStore::save(const Queue& queue) const
    {
        QSaveFile file(mFilePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            return false;
        }

        file.write(QueueStore::serialize(queue));
        return file.commit();
    }
}