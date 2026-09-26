#ifndef SYNCPREVIEWFAKEAPPLIER_H
#define SYNCPREVIEWFAKEAPPLIER_H

#include "FakeSyncPreviewProvider.h"
#include "SyncPreviewPlanner.h"

namespace SyncPreview
{
    // Executes a plan's scheduled operations against a fake scenario's two
    // trees (MEGA-2.9 review loop: apply → inspect → adjust → apply again).
    // Pure model function over the fake trees: uploads/downloads copy the
    // source-side entry to the destination side (missing ancestor folders
    // are created), replaces overwrite the destination entry, deletions
    // remove the entry with its subtree, renames move entries to their new
    // path (a same-row rename pair forming a swap applies as one atomic
    // permutation, so neither copy is lost). Same operation vocabulary the
    // Stage 4 enforcement engine will expand into real primitives; nothing
    // here touches real APIs or the filesystem.
    void applyPlan(FakeScenario& scenario, const Plan& plan);
}

#endif // SYNCPREVIEWFAKEAPPLIER_H