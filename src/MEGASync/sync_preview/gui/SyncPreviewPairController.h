#ifndef SYNCPREVIEWPAIRCONTROLLER_H
#define SYNCPREVIEWPAIRCONTROLLER_H

#include "SyncPreviewClassifier.h"
#include "SyncPreviewPlanner.h"
#include "SyncPreviewQueue.h"
#include "SyncPreviewQueueFileStore.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QVector>

#include <functional>
#include <memory>
#include <optional>

namespace SyncPreview
{
    // What a pair source hands over when the user adds a pair. Stage 2 fills
    // it with fake-scenario labels; the Stage 5 picker fills it with real
    // local/remote paths.
    struct PairCandidate
    {
        QString localPath;    // local root display path
        QString remotePath;   // remote root display path
        QString remoteHandle; // remote root node handle ("" on fake data)
    };

    // Both sides of one pair, built fresh for every (re-)scan. The factory
    // maps a persisted pair back to live providers: fake scenarios in Stage
    // 2, real providers from Stage 3 on.
    struct PairSideProviders
    {
        std::shared_ptr<LocalSideProvider> local;
        std::shared_ptr<RemoteSideProvider> remote;
    };

    using SideProviderFactory = std::function<std::optional<PairSideProviders>(const Pair&)>;

    // Applies a plan's scheduled changes to the pair's underlying data
    // (MEGA-2.9 review loop). Stage 2 installs the fake-data applier
    // (mutates the fake trees); the Stage 4 enforcement engine plugs in at
    // the same seam later. Returns false when the plan could not be
    // applied.
    using PlanApplier = std::function<bool(const QString& pairId, const Plan& plan)>;

    // Per-pair roll-up for the pair-first list view (MEGA-2.7): whole-subtree
    // stats per side from the classification, plus the pending-transfer delta
    // per side from the current plan (what the commit would actually
    // transfer or remove under the current decisions, recommended actions
    // included). Pure additive API; no state is touched by computing it.
    struct PairSummary
    {
        // Whole subtree per side.
        qint64 localBytes = 0;
        int localFiles = 0;
        int localDirs = 0;
        qint64 remoteBytes = 0;
        int remoteFiles = 0;
        int remoteDirs = 0;

        // Pending delta per side. pending*Bytes sums the source-side sizes
        // of the created+changed file entries that would transfer (upload
        // carries local bytes, download remote bytes); folder nodes carry
        // none. pending*Removed counts the paths that would be trashed
        // (local) or moved to MEGA Rubbish (remote).
        qint64 pendingLocalBytes = 0;
        int pendingLocalFiles = 0;
        int pendingLocalRemoved = 0;
        qint64 pendingRemoteBytes = 0;
        int pendingRemoteFiles = 0;
        int pendingRemoteRemoved = 0;
    };

    // Headless orchestrator behind the review dialog: the pair queue,
    // per-pair classification, per-row decisions and approvals, plans, the
    // approval gate, and queue persistence. Owns no widgets, so the decision
    // logic is unit-testable; the dialog is a thin view on top.
    //
    // The queue is persisted (QueueFileStore) after every mutation and
    // re-verified on restore: the Reconciler drops decisions for vanished
    // rows, clears approvals on rows whose classification changed, and drops
    // completed pairs.
    class PairController : public QObject
    {
        Q_OBJECT

    public:
        explicit PairController(QueueFileStore store, QObject* parent = nullptr);

        // Installs the pair source (fake scenarios / real picker).
        void setSideProviderFactory(SideProviderFactory factory);

        // Installs the plan applier used by applyPlan (fake data in
        // Stage 2, the enforcement engine from Stage 4 on).
        void setPlanApplier(PlanApplier applier);

        // Loads the persisted queue, re-scans every pair and re-verifies the
        // decisions (Reconciler), then persists the reconciled state.
        void restore();

        const QVector<Pair>& pairs() const { return mQueue.pairs; }
        const Pair* pair(const QString& pairId) const;

        // The pair's latest classification (empty when not yet scanned).
        const Classification& classification(const QString& pairId) const;

        // Paths re-flagged during the last restore (classification changed →
        // approval cleared).
        QStringList reFlaggedPaths(const QString& pairId) const;

        // The plan for the pair's current decisions (Planner semantics:
        // own decision > ancestor directory decision > recommended).
        Plan plan(const QString& pairId) const;

        // plan() with one hypothetical decision applied (used for the
        // directory-action consequences preview).
        Plan previewPlan(const QString& pairId, const QString& relativePath, Action action) const;

        // Whole-subtree stats plus the pending-transfer delta for the pair
        // (see PairSummary). Zeros for a pair without a classification.
        PairSummary summary(const QString& pairId) const;

        void setAction(const QString& pairId, const QString& relativePath, Action action);
        void setApproved(const QString& pairId, const QString& relativePath, bool approved);

        void addPair(const PairCandidate& candidate);
        void removePair(const QString& pairId);

        // Review loop (MEGA-2.9): executes the pair's current plan through
        // the installed applier, then re-scans and re-verifies the pair
        // (decisions on rows the plan made vanish are dropped) so the
        // reviewer can apply → inspect → adjust → apply again. Not gated
        // on approvals: undecided flagged rows contribute no operations.
        bool applyPlan(const QString& pairId);

        // Mocked commit (MEGA-2.10; the Stage-5 real commit flow replaces
        // this at the same seam): gated on the approval gate, applies the
        // plan through the installed applier (fake data), then drops the
        // pair from the queue — "the sync was created" in the mock.
        bool commitPair(const QString& pairId);

        // Commit gate: every row requiring approval must be approved. A row
        // the user explicitly resolved to "do nothing" (own decision or
        // inherited from a directory decision) does not await approval;
        // undecided conflict/blocker rows (recommended action None) still do.
        int awaitingApprovalCount(const QString& pairId) const;
        QStringList awaitingApprovalPaths(const QString& pairId) const;
        bool allApproved(const QString& pairId) const;

        const QString& lastError() const { return mLastError; }

    signals:
        void pairAdded(const QString& pairId);
        void pairRemoved(const QString& pairId);
        void pairChanged(const QString& pairId);  // decisions or classification changed
        void restored();

    private:
        bool rescanPair(Pair& pair);
        void persist();

        QueueFileStore mStore;
        SideProviderFactory mSideProviderFactory;
        PlanApplier mPlanApplier;
        Queue mQueue;
        QHash<QString, Classification> mClassifications;
        QHash<QString, QStringList> mReFlagged;
        QString mLastError;
    };
}
#endif // SYNCPREVIEWPAIRCONTROLLER_H