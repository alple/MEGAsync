#ifndef SYNCPREVIEWPROVIDERS_H
#define SYNCPREVIEWPROVIDERS_H

#include "SyncPreviewTree.h"

namespace SyncPreview
{
    // Local side of the provider seam. The comparison/planning core depends
    // only on these interfaces, never on the filesystem, the SDK, or any
    // concrete data source. Stage 3 plugs real providers (local walk) into
    // the same seam; the fake provider (Stage 1) implements both sides.
    class LocalSideProvider
    {
    public:
        virtual ~LocalSideProvider() = default;

        // Materialized snapshot of the local folder tree.
        virtual Tree snapshot() const = 0;
    };

    // Remote side of the provider seam. Stage 3 implements this by
    // enumerating the node cache (getChildren per folder, ORDER_NONE fast
    // path, paged variant for huge trees).
    class RemoteSideProvider
    {
    public:
        virtual ~RemoteSideProvider() = default;

        // Materialized snapshot of the remote node tree.
        virtual Tree snapshot() const = 0;
    };
}

#endif // SYNCPREVIEWPROVIDERS_H
