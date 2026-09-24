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
        // (five-level) identical subtree.
        FakeScenario edgeCaseKitchenSink();

        // Identical chain of `depth` nested folders ending in one identical
        // file on both sides.
        FakeScenario deepTree(int depth);

        // Both sides empty (no rows).
        FakeScenario emptySides();
    }
}

#endif // FAKESYNCPREVIEWPROVIDER_H
