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

        void setAction(const QString& pairId, const QString& relativePath, Action action);
        void setApproved(const QString& pairId, const QString& relativePath, bool approved);

        void addPair(const PairCandidate& candidate);
        void removePair(const QString& pairId);

        // Commit gate: every row requiring approval must be approved.
        int awaitingApprovalCount(const QString& pairId) const;
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
        Queue mQueue;
        QHash<QString, Classification> mClassifications;
        QHash<QString, QStringList> mReFlagged;
        QString mLastError;
    };
}
#endif // SYNCPREVIEWPAIRCONTROLLER_H