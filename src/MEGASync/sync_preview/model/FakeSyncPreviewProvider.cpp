#include "FakeSyncPreviewProvider.h"

#include <QHash>
#include <utility>

namespace SyncPreview
{
    FakeTreeBuilder& FakeTreeBuilder::addFolder(const QString& relativePath)
    {
        if (mTree.contains(relativePath))
        {
            return *this;
        }
        ensureAncestors(relativePath);

        Entry folder;
        folder.relativePath = relativePath;
        folder.type = EntryType::Folder;
        mTree.addEntry(folder);
        return *this;
    }

    FakeTreeBuilder& FakeTreeBuilder::addFile(const QString& relativePath, qint64 size, qint64 modifiedTime, const QByteArray& contentHash)
    {
        ensureAncestors(relativePath);

        Entry file;
        file.relativePath = relativePath;
        file.type = EntryType::File;
        file.size = size;
        file.modifiedTime = modifiedTime;
        file.contentHash = contentHash;
        mTree.addEntry(file);
        return *this;
    }

    FakeTreeBuilder& FakeTreeBuilder::addFile(const QString& relativePath, qint64 size, qint64 modifiedTime, const char* contentTag)
    {
        return addFile(relativePath, size, modifiedTime, QByteArray(contentTag));
    }

    Tree FakeTreeBuilder::build() const
    {
        return mTree;
    }

    void FakeTreeBuilder::ensureAncestors(const QString& relativePath)
    {
        const QStringList parts = splitPath(relativePath);
        QString current;
        for (int i = 0; i < parts.size() - 1; ++i)
        {
            current = appendPath(current, parts.at(i));
            addFolder(current);
        }
    }

    FakeSideProvider::FakeSideProvider(Tree tree)
        : mTree(std::move(tree))
    {
    }

    Tree FakeSideProvider::snapshot() const
    {
        return mTree;
    }

    FakeScenario FakeScenarios::edgeCaseKitchenSink()
    {
        FakeTreeBuilder localBuilder;
        FakeTreeBuilder remoteBuilder;

        // Identical nested subtree.
        localBuilder.addFolder(QStringLiteral("docs"));
        localBuilder.addFolder(QStringLiteral("docs/plans"));
        localBuilder.addFile(QStringLiteral("docs/plans/q1.txt"), 100, 1000, "q1");
        localBuilder.addFile(QStringLiteral("docs/report.txt"), 200, 1100, "report");

        remoteBuilder.addFolder(QStringLiteral("docs"));
        remoteBuilder.addFolder(QStringLiteral("docs/plans"));
        remoteBuilder.addFile(QStringLiteral("docs/plans/q1.txt"), 100, 1000, "q1");
        remoteBuilder.addFile(QStringLiteral("docs/report.txt"), 200, 1100, "report");

        // Same name, different content (one size mismatch, one CRC-only
        // mismatch with equal sizes).
        localBuilder.addFolder(QStringLiteral("photos"));
        localBuilder.addFile(QStringLiteral("photos/img1.png"), 300, 2000, "img1-local");
        localBuilder.addFile(QStringLiteral("photos/img2.png"), 400, 2100, "img2-local");

        remoteBuilder.addFolder(QStringLiteral("photos"));
        remoteBuilder.addFile(QStringLiteral("photos/img1.png"), 300, 2500, "img1-remote");
        remoteBuilder.addFile(QStringLiteral("photos/img2.png"), 450, 2100, "img2-remote");

        // Same content, different names (conflict twins).
        localBuilder.addFolder(QStringLiteral("archive"));
        localBuilder.addFile(QStringLiteral("archive/a.txt"), 50, 500, "twin");

        remoteBuilder.addFolder(QStringLiteral("old"));
        remoteBuilder.addFile(QStringLiteral("old/b.txt"), 50, 500, "twin");

        // File-vs-folder type mismatch: local file, remote folder with a
        // child inside (the child must surface under a blocked path).
        localBuilder.addFile(QStringLiteral("misc/notes.txt"), 15, 300, "notes-local");

        remoteBuilder.addFile(QStringLiteral("misc/notes.txt/draft.txt"), 10, 300, "draft");

        // Case-insensitive name collision.
        localBuilder.addFolder(QStringLiteral("notes"));
        localBuilder.addFile(QStringLiteral("notes/A.txt"), 60, 600, "collision-local");

        remoteBuilder.addFolder(QStringLiteral("notes"));
        remoteBuilder.addFile(QStringLiteral("notes/a.txt"), 70, 600, "collision-remote");

        // Empty dirs, one per side.
        localBuilder.addFolder(QStringLiteral("empty_local"));
        remoteBuilder.addFolder(QStringLiteral("empty_remote"));

        // Single-sided files.
        localBuilder.addFile(QStringLiteral("only_local.txt"), 20, 700, "only-local");
        remoteBuilder.addFile(QStringLiteral("only_remote.txt"), 25, 800, "only-remote");

        // Deep identical subtree (5 levels).
        localBuilder.addFolder(QStringLiteral("deep/l2/l3/l4/l5"));
        localBuilder.addFile(QStringLiteral("deep/l2/l3/l4/l5/deep.txt"), 5, 900, "deep");
        remoteBuilder.addFolder(QStringLiteral("deep/l2/l3/l4/l5"));
        remoteBuilder.addFile(QStringLiteral("deep/l2/l3/l4/l5/deep.txt"), 5, 900, "deep");

        return {localBuilder.build(), remoteBuilder.build()};
    }

    FakeScenario FakeScenarios::deepTree(int depth)
    {
        FakeTreeBuilder localBuilder;
        FakeTreeBuilder remoteBuilder;

        QString localPath;
        QString remotePath;
        for (int level = 1; level <= depth; ++level)
        {
            localPath = appendPath(localPath, QStringLiteral("level%1").arg(level));
            remotePath = appendPath(remotePath, QStringLiteral("level%1").arg(level));
            localBuilder.addFolder(localPath);
            remoteBuilder.addFolder(remotePath);
        }

        const QString leafPath = appendPath(localPath, QStringLiteral("leaf.txt"));
        localBuilder.addFile(leafPath, 1, 1, "leaf");
        remoteBuilder.addFile(leafPath, 1, 1, "leaf");

        return {localBuilder.build(), remoteBuilder.build()};
    }

    FakeScenario FakeScenarios::emptySides()
    {
        return {};
    }
}
