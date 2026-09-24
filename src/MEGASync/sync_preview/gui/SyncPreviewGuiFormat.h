#ifndef SYNCPREVIEWGUIFORMAT_H
#define SYNCPREVIEWGUIFORMAT_H

#include "SyncPreviewTree.h"

#include <QDateTime>
#include <QString>

#include <optional>

namespace SyncPreview
{
    namespace GuiText
    {
        // Display helpers shared by the pair list and the MC-style detail
        // window (MEGA-2.7). Root-relative paths use '/' separators.

        inline QString sizeBytesText(qint64 bytes)
        {
            if (bytes < 1024)
            {
                return QStringLiteral("%1 B").arg(bytes);
            }
            const double kb = static_cast<double>(bytes) / 1024.0;
            if (kb < 1024.0)
            {
                return QStringLiteral("%1 KB").arg(kb, 0, 'f', 1);
            }
            return QStringLiteral("%1 MB").arg(kb / 1024.0, 0, 'f', 1);
        }

        inline QString sizeText(const std::optional<Entry>& entry)
        {
            if (!entry || entry->isFolder())
            {
                return QStringLiteral("-");
            }
            return sizeBytesText(entry->size);
        }

        inline QString timeText(const std::optional<Entry>& entry)
        {
            if (!entry || entry->modifiedTime <= 0)
            {
                return QStringLiteral("-");
            }
            const QDateTime dateTime = QDateTime::fromSecsSinceEpoch(entry->modifiedTime);
            return dateTime.date().toString(QStringLiteral("yyyy-MM-dd")) + QLatin1Char(' ') +
                dateTime.time().toString(QStringLiteral("HH:mm"));
        }

        // Two-space indent per path depth, mirroring the tree nesting.
        inline QString indentFor(const QString& relativePath)
        {
            const int depth = relativePath.count(QLatin1Char('/'));
            return QString(2 * depth, QLatin1Char(' '));
        }
    }
}
#endif // SYNCPREVIEWGUIFORMAT_H