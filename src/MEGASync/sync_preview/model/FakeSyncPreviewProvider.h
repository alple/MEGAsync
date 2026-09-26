#ifndef FAKESYNCPREVIEWPROVIDER_H
#define FAKESYNCPREVIEWPROVIDER_H

#include "SyncPreviewProviders.h"

namespace SyncPreview
{
    // Fluent builder for fake tree snapshots. Parent folders are created
    // automatically; content hashes are opaque equality tokens (the fake
    // layer never computes real CRCs).
    class FakeTreeBuilder
    {
    public:
        FakeTreeBuilder& addFolder(const QString& relativePath);
        FakeTreeBuilder& addFile(const QString& relativePath, qint64 size, qint64 modifiedTime, const QByteArray& contentHash);
        // Convenience: the content tag string doubles as the equality token.
        FakeTreeBuilder& addFile(const QString& relativePath, qint64 size, qint64 modifiedTime, const char* contentTag);
        Tree build() const;

    private:
        void ensureAncestors(const QString& relativePath);
        Tree mTree;
    };

    // In-memory provider implementing both sides of the seam; construct two
    // instances (one per side) for a comparison. Stage 2 drives the review
    // dialog on these; Stage 3 replaces them with real providers.
    class FakeSideProvider : public LocalSideProvider, public RemoteSideProvider
    {
    public:
        explicit FakeSideProvider(Tree tree);
        Tree snapshot() const override;

    private:
        Tree mTree;
    };

    // A pair of trees handed to the classifier/planner via two
    // FakeSideProvider instances.
    struct FakeScenario
    {
        Tree local;
        Tree remote;
    };

    // Canned scenarios covering the acceptance-criteria edge cases.
    namespace FakeScenarios
    {
        // Single pair of trees covering: nested directories, empty dirs on
        // both sides, same-name-different-content, same-content-different-
        // name, file-vs-folder type mismatch, case-insensitive name
        // collision, local-only and remote-only files, and a deep
        // (five-level) identical subtree. Also carries a nested
        // rename-twin branch (MEGA-2.9): a remote backup copy of the
        // local main.cpp content gives the modified src/main.cpp row an
        // identical-content remote counterpart.
        FakeScenario edgeCaseKitchenSink();

        // Identical chain of `depth` nested folders ending in one identical
        // file on both sides.
        FakeScenario deepTree(int depth);

        // Both sides empty (no rows).
        FakeScenario emptySides();

        // `count` local-only files (plus one remote-only file) in the root:
        // a long visible list exercising the row cap and load-more.
        FakeScenario manyLocalOnlyFiles(int count);

        // The rename-swap trap: remote renamed foo.txt to bar.txt (old
        // content under the new name) while remote foo.txt holds new
        // content. A naive L->R transfer overwrites content the remote
        // already keeps under bar.txt; the rename-aware swap exchanges the
        // two names instead (MEGA-2.9 acceptance fixture).
        FakeScenario renameSwap();

        // Double rename, both directions: remote renamed foo.txt to
        // bar.txt AND foo2.txt to foo.txt, so the paired foo.txt row has
        // same-content counterparts on BOTH sides (bar.txt remotely,
        // foo2.txt locally) — either arrow can resolve by rename.
        FakeScenario renameChain();

        // Rename + edit on top: remote renamed the original foo.txt to
        // bar.txt and created a different foo.txt, while local edited
        // foo.txt. No same-content counterpart exists anywhere, so no
        // rename-aware resolution applies: the naive replace transfers
        // with the unique-content advisory warning.
        FakeScenario renameEdit();

        // Rename across folders: remote moved docs/a.txt to archive/a.txt
        // (old content under the new name) and replaced docs/a.txt with
        // new content, while local kept docs/a.txt. The paired row's twin
        // sits in another folder, so the rename-aware swap moves content
        // across folders.
        FakeScenario renameCrossFolder();
    }
}

#endif // FAKESYNCPREVIEWPROVIDER_H
