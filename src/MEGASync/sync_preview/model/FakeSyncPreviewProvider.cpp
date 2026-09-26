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

        // Nested rename twin (MEGA-2.9): the remote keeps a backup copy of
        // the local main.cpp content under a different name, so the
        // modified src/main.cpp row carries an identical-content remote
        // counterpart. Added before the twins so the remote-leftover tail
        // order stays stable.
        remoteBuilder.addFolder(QStringLiteral("projects/mega/client/backup"));
        remoteBuilder.addFile(QStringLiteral("projects/mega/client/backup/main.cpp"), 500, 3000, "main-local");

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

        // Nested working tree, four levels deep, mixed states in one
        // branch: identical at the top, a CRC-only modification at the
        // bottom (the folders along the path read Modified).
        localBuilder.addFolder(QStringLiteral("projects/mega/client/src"));
        localBuilder.addFile(QStringLiteral("projects/mega/client/src/main.cpp"), 500, 3000, "main-local");
        localBuilder.addFile(QStringLiteral("projects/mega/client/src/util.h"), 120, 3100, "util");
        remoteBuilder.addFolder(QStringLiteral("projects/mega/client/src"));
        remoteBuilder.addFile(QStringLiteral("projects/mega/client/src/main.cpp"), 500, 3500, "main-remote");
        remoteBuilder.addFile(QStringLiteral("projects/mega/client/src/util.h"), 120, 3100, "util");

        return {localBuilder.build(), remoteBuilder.build()};
    }

    FakeScenario FakeScenarios::renameSwap()
    {
        FakeTreeBuilder localBuilder;
        FakeTreeBuilder remoteBuilder;

        // The rename-swap trap: remote renamed foo.txt to bar.txt (the old
        // content survives under the new name) while remote foo.txt holds
        // new content. A naive L->R transfer would overwrite the remote
        // edit with content the remote already keeps under bar.txt; the
        // honest resolution is the rename-aware swap.
        localBuilder.addFile(QStringLiteral("foo.txt"), 100, 1000, "hash_1");
        remoteBuilder.addFile(QStringLiteral("foo.txt"), 200, 2000, "hash_2");
        remoteBuilder.addFile(QStringLiteral("bar.txt"), 100, 1000, "hash_1");

        return {localBuilder.build(), remoteBuilder.build()};
    }

    FakeScenario FakeScenarios::renameChain()
    {
        FakeTreeBuilder localBuilder;
        FakeTreeBuilder remoteBuilder;

        // Double rename on the remote side: foo.txt -> bar.txt and
        // foo2.txt -> foo.txt. The paired foo.txt row differs on both
        // sides and finds same-content counterparts in BOTH directions:
        // its local content sits remotely at bar.txt (L->R adopt), its
        // remote content sits locally at foo2.txt (R->L adopt).
        localBuilder.addFile(QStringLiteral("foo.txt"), 100, 1000, "hash_1");
        localBuilder.addFile(QStringLiteral("foo2.txt"), 50, 1100, "hash_2");
        remoteBuilder.addFile(QStringLiteral("foo.txt"), 50, 1100, "hash_2");
        remoteBuilder.addFile(QStringLiteral("bar.txt"), 100, 1000, "hash_1");

        return {localBuilder.build(), remoteBuilder.build()};
    }

    FakeScenario FakeScenarios::renameEdit()
    {
        FakeTreeBuilder localBuilder;
        FakeTreeBuilder remoteBuilder;

        // Rename + edit: remote renamed the original foo.txt (hash_1) to
        // bar.txt and created a different foo.txt (hash_2); local edited
        // foo.txt (hash_1e). Three distinct contents with no identical
        // counterpart for the local edit: no rename-aware resolution
        // applies, so a transfer is the plain recoverable replace (with
        // the unique-content advisory when the replaced side keeps the
        // only copy).
        localBuilder.addFile(QStringLiteral("foo.txt"), 120, 1500, "hash_1e");
        remoteBuilder.addFile(QStringLiteral("foo.txt"), 200, 2000, "hash_2");
        remoteBuilder.addFile(QStringLiteral("bar.txt"), 100, 1000, "hash_1");

        return {localBuilder.build(), remoteBuilder.build()};
    }

    FakeScenario FakeScenarios::renameCrossFolder()
    {
        FakeTreeBuilder localBuilder;
        FakeTreeBuilder remoteBuilder;

        // Rename across folders: remote moved docs/a.txt to archive/a.txt
        // (the old content under the new name) and replaced docs/a.txt
        // with new content; local kept docs/a.txt. The paired row's twin
        // sits in another folder, so the rename-aware swap moves content
        // across folders.
        localBuilder.addFolder(QStringLiteral("docs"));
        localBuilder.addFile(QStringLiteral("docs/a.txt"), 100, 1000, "hash_1");
        remoteBuilder.addFolder(QStringLiteral("docs"));
        remoteBuilder.addFile(QStringLiteral("docs/a.txt"), 150, 2000, "hash_2");
        remoteBuilder.addFolder(QStringLiteral("archive"));
        remoteBuilder.addFile(QStringLiteral("archive/a.txt"), 100, 1000, "hash_1");

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

    FakeScenario FakeScenarios::manyLocalOnlyFiles(int count)
    {
        FakeTreeBuilder localBuilder;
        FakeTreeBuilder remoteBuilder;

        for (int i = 0; i < count; ++i)
        {
            localBuilder.addFile(QStringLiteral("file-%1.txt").arg(i, 4, 10, QLatin1Char('0')),
                                 100 + i,
                                 1000 + i,
                                 QStringLiteral("local-only-%1").arg(i).toUtf8());
        }
        remoteBuilder.addFile(QStringLiteral("remote-only.txt"), 25, 800, "only-remote");

        return {localBuilder.build(), remoteBuilder.build()};
    }
}
