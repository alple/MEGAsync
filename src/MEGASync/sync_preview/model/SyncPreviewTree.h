#ifndef SYNCPREVIEWTREE_H
#define SYNCPREVIEWTREE_H

#include "SyncPreviewDefs.h"

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QVector>

namespace SyncPreview
{
    // One file or folder in a side's tree snapshot, keyed by its
    // root-relative path ('/'-separated, no leading or trailing separator).
    struct Entry
    {
        QString relativePath;
        EntryType type = EntryType::File;
        qint64 size = 0;          // files only; always 0 for folders
        qint64 modifiedTime = 0;  // epoch seconds
        QByteArray contentHash;   // CRC (local getCRC / remote
                                  // getCRCFromFingerprint); empty for folders

        bool isFolder() const { return type == EntryType::Folder; }
    };

    // Materialized snapshot of one side's tree, in stable tree order
    // (parents before children). The comparison and planning core consumes
    // snapshots produced through the provider interfaces; it never walks
    // real filesystems or the SDK node cache itself.
    class Tree
    {
    public:
        // Adds an entry. Entries may be added in any order; callers
        // building a displayable tree add folders before their children.
        void addEntry(const Entry& entry);

        const QVector<Entry>& entries() const { return mEntries; }
        int size() const { return mEntries.size(); }
        bool isEmpty() const { return mEntries.isEmpty(); }

        // Exact, case-sensitive lookup by relative path.
        const Entry* find(const QString& relativePath) const;
        bool contains(const QString& relativePath) const { return find(relativePath) != nullptr; }

        void clear();

    private:
        QVector<Entry> mEntries;
        QHash<QString, int> mIndexByPath;
    };

    // Content equality with size short-circuit, then CRC comparison. Folder
    // entries never compare as same content; entries with a size mismatch
    // are different without consulting CRCs; a missing CRC on either side
    // makes the entries different (conservative: they resurface for a
    // re-check rather than silently passing as identical).
    bool sameContent(const Entry& lhs, const Entry& rhs);

    // Path helpers. Relative paths use '/' as separator; the root has no
    // representation (root-relative only).
    QStringList splitPath(const QString& relativePath);
    QString parentPath(const QString& relativePath);
    QString appendPath(const QString& parentPath, const QString& name);
    // Case-insensitive matching key. Local filesystems are typically
    // case-insensitive; MEGA's remote namespace is case-sensitive, so
    // collisions between spellings that differ only in case need explicit
    // detection via this key.
    QString caseInsensitiveKey(const QString& relativePath);
}

#endif // SYNCPREVIEWTREE_H
