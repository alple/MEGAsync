#include "SyncPreviewTree.h"

#include <QStringList>

namespace SyncPreview
{
    void Tree::addEntry(const Entry& entry)
    {
        const int index = static_cast<int>(mEntries.size());
        mEntries.append(entry);
        mIndexByPath.insert(entry.relativePath, index);
    }

    const Entry* Tree::find(const QString& relativePath) const
    {
        const auto it = mIndexByPath.constFind(relativePath);
        if (it == mIndexByPath.constEnd() || it.value() < 0 || it.value() >= mEntries.size())
        {
            return nullptr;
        }
        return &mEntries.at(it.value());
    }

    void Tree::clear()
    {
        mEntries.clear();
        mIndexByPath.clear();
    }

    bool sameContent(const Entry& lhs, const Entry& rhs)
    {
        if (lhs.isFolder() || rhs.isFolder())
        {
            return false;
        }

        // Size short-circuit: sizes differ -> content differs.
        if (lhs.size != rhs.size)
        {
            return false;
        }

        // CRC is the content authority. Missing CRCs on either side are
        // treated as unknown, not equal.
        if (lhs.contentHash.isEmpty() || rhs.contentHash.isEmpty())
        {
            return false;
        }

        return lhs.contentHash == rhs.contentHash;
    }

    QStringList splitPath(const QString& relativePath)
    {
        if (relativePath.isEmpty())
        {
            return {};
        }
        return relativePath.split(QLatin1Char('/'), Qt::KeepEmptyParts);
    }

    QString parentPath(const QString& relativePath)
    {
        const int lastSeparator = relativePath.lastIndexOf(QLatin1Char('/'));
        if (lastSeparator < 0)
        {
            return QString();
        }
        return relativePath.left(lastSeparator);
    }

    QString appendPath(const QString& parentPath, const QString& name)
    {
        if (parentPath.isEmpty())
        {
            return name;
        }
        return parentPath + QLatin1Char('/') + name;
    }

    QString caseInsensitiveKey(const QString& relativePath)
    {
        return relativePath.toLower();
    }
}
