#include "SyncPreviewPairController.h"
#include "SyncPreviewQueueStore.h"
#include "SyncPreviewReconciler.h"

#include <QSet>
#include <QUuid>

namespace SyncPreview
{
    PairController::PairController(QueueFileStore store, QObject* parent) :
        QObject(parent),
        mStore(std::move(store))
    {
    }

    void PairController::setSideProviderFactory(SideProviderFactory factory)
    {
        mSideProviderFactory = std::move(factory);
    }

    void PairController::setPlanApplier(PlanApplier applier)
    {
        mPlanApplier = std::move(applier);
    }

    void PairController::restore()
    {
        Queue loaded = mStore.load();
        QHash<QString, Classification> fresh;

        for (Pair& pair : loaded.pairs)
        {
            if (rescanPair(pair))
            {
                fresh.insert(pair.id, mClassifications.value(pair.id));
            }
        }

        const ReconcileResult result = Reconciler().reconcile(loaded, fresh);
        mQueue = result.queue;
        mReFlagged = result.reFlaggedPaths;

        // Pairs that survived reconcile but were not re-scanned (no factory
        // or provider failure) keep no stale classification.
        for (const Pair& pair : mQueue.pairs)
        {
            if (!mClassifications.contains(pair.id))
            {
                mClassifications.insert(pair.id, Classification());
            }
        }

        persist();
        emit restored();
    }

    const Pair* PairController::pair(const QString& pairId) const
    {
        for (const Pair& pair : mQueue.pairs)
        {
            if (pair.id == pairId)
            {
                return &pair;
            }
        }
        return nullptr;
    }

    const Classification& PairController::classification(const QString& pairId) const
    {
        static const Classification empty;
        const auto it = mClassifications.constFind(pairId);
        return it != mClassifications.constEnd() ? it.value() : empty;
    }

    QStringList PairController::reFlaggedPaths(const QString& pairId) const
    {
        return mReFlagged.value(pairId);
    }

    Plan PairController::plan(const QString& pairId) const
    {
        Plan noPlan;
        const Pair* pairPtr = pair(pairId);
        const auto classificationIt = mClassifications.constFind(pairId);
        if (!pairPtr || classificationIt == mClassifications.constEnd())
        {
            return noPlan;
        }

        QHash<QString, Action> decisions;
        for (const RowDecision& decision : pairPtr->decisions)
        {
            decisions.insert(decision.relativePath, decision.action);
        }

        static const Planner planner;
        return planner.plan(classificationIt.value(), decisions);
    }

    Plan PairController::previewPlan(const QString& pairId, const QString& relativePath, Action action) const
    {
        Plan noPlan;
        const Pair* pairPtr = pair(pairId);
        const auto classificationIt = mClassifications.constFind(pairId);
        if (!pairPtr || classificationIt == mClassifications.constEnd())
        {
            return noPlan;
        }

        QHash<QString, Action> decisions;
        for (const RowDecision& decision : pairPtr->decisions)
        {
            decisions.insert(decision.relativePath, decision.action);
        }
        decisions.insert(relativePath, action);

        static const Planner planner;
        return planner.plan(classificationIt.value(), decisions);
    }

    PairSummary PairController::summary(const QString& pairId) const
    {
        PairSummary summary;
        const Classification& classification = this->classification(pairId);

        // Whole subtree per side. Conflict rows hold exactly one side's
        // entry, blocker rows hold both, paired rows both equal copies:
        // every side's entry is counted exactly once for its side.
        for (const Row& row : classification.rows)
        {
            if (row.local)
            {
                if (row.local->isFolder())
                {
                    ++summary.localDirs;
                }
                else
                {
                    ++summary.localFiles;
                    summary.localBytes += row.local->size;
                }
            }
            if (row.remote)
            {
                if (row.remote->isFolder())
                {
                    ++summary.remoteDirs;
                }
                else
                {
                    ++summary.remoteFiles;
                    summary.remoteBytes += row.remote->size;
                }
            }
        }

        // Pending delta from the plan. Consequence paths repeat across rows
        // (a directory row aggregates the consequences of its self-planning
        // descendants), so dedupe per consequence type before counting.
        const Plan pairPlan = plan(pairId);
        QSet<QString> createdLocal;
        QSet<QString> changedLocal;
        QSet<QString> removedLocal;
        QSet<QString> createdRemote;
        QSet<QString> changedRemote;
        QSet<QString> removedRemote;
        for (const RowPlan& rowPlan : pairPlan.rows)
        {
            for (const QString& path : rowPlan.createdLocal) { createdLocal.insert(path); }
            for (const QString& path : rowPlan.changedLocal) { changedLocal.insert(path); }
            for (const QString& path : rowPlan.removedLocal) { removedLocal.insert(path); }
            for (const QString& path : rowPlan.createdRemote) { createdRemote.insert(path); }
            for (const QString& path : rowPlan.changedRemote) { changedRemote.insert(path); }
            for (const QString& path : rowPlan.removedRemote) { removedRemote.insert(path); }
        }

        auto accumulate = [&classification](const QSet<QString>& paths,
                                            bool sourceIsLocal,
                                            qint64& bytes,
                                            int& files)
        {
            for (const QString& path : paths)
            {
                const Row* row = classification.find(path);
                if (!row)
                {
                    continue;
                }
                const std::optional<Entry>& source = sourceIsLocal ? row->local : row->remote;
                if (!source || source->isFolder())
                {
                    continue;
                }
                bytes += source->size;
                ++files;
            }
        };

        // Uploads carry local bytes to the remote side; downloads the
        // reverse. Folder nodes and removals carry no transfer bytes.
        accumulate(createdRemote, true, summary.pendingRemoteBytes, summary.pendingRemoteFiles);
        accumulate(changedRemote, true, summary.pendingRemoteBytes, summary.pendingRemoteFiles);
        accumulate(createdLocal, false, summary.pendingLocalBytes, summary.pendingLocalFiles);
        accumulate(changedLocal, false, summary.pendingLocalBytes, summary.pendingLocalFiles);
        summary.pendingLocalRemoved = removedLocal.size();
        summary.pendingRemoteRemoved = removedRemote.size();

        return summary;
    }

    void PairController::setAction(const QString& pairId, const QString& relativePath, Action action)
    {
        Pair* pairPtr = nullptr;
        for (Pair& pair : mQueue.pairs)
        {
            if (pair.id == pairId)
            {
                pairPtr = &pair;
                break;
            }
        }
        if (!pairPtr)
        {
            return;
        }

        const auto classificationIt = mClassifications.constFind(pairId);
        const Row* row = classificationIt != mClassifications.constEnd()
            ? classificationIt->find(relativePath) : nullptr;

        for (RowDecision& decision : pairPtr->decisions)
        {
            if (decision.relativePath == relativePath)
            {
                decision.action = action;
                if (row)
                {
                    // Refresh the snapshots so the next re-verify does not
                    // mistake this decision for a stale one.
                    decision.kind = static_cast<int>(row->kind);
                    decision.requiresApproval = row->requiresApproval;
                }
                persist();
                emit pairChanged(pairId);
                return;
            }
        }

        RowDecision decision;
        decision.relativePath = relativePath;
        decision.action = action;
        decision.approved = false;
        decision.kind = row ? static_cast<int>(row->kind) : RowDecision::UNKNOWN_KIND;
        decision.requiresApproval = row ? row->requiresApproval : false;
        pairPtr->decisions.append(decision);
        persist();
        emit pairChanged(pairId);
    }

    void PairController::setApproved(const QString& pairId, const QString& relativePath, bool approved)
    {
        Pair* pairPtr = nullptr;
        for (Pair& pair : mQueue.pairs)
        {
            if (pair.id == pairId)
            {
                pairPtr = &pair;
                break;
            }
        }
        if (!pairPtr)
        {
            return;
        }

        // Approval attaches to an EXISTING decision only (MEGA-2.10): the
        // UI offers the Approve toggle when a row's effective action is
        // decidable — a chosen action — so an undecided flagged row has
        // nothing to approve. Creating approve-only decisions here used to
        // make the planner read them as explicit choices (killing the
        // directory cascade and emptying the commit plan); the reviewer
        // decides first, then approves.
        for (RowDecision& decision : pairPtr->decisions)
        {
            if (decision.relativePath == relativePath)
            {
                decision.approved = approved;

                const auto classificationIt = mClassifications.constFind(pairId);
                const Row* row = classificationIt != mClassifications.constEnd()
                    ? classificationIt->find(relativePath) : nullptr;
                if (row)
                {
                    decision.kind = static_cast<int>(row->kind);
                    decision.requiresApproval = row->requiresApproval;
                }
                persist();
                emit pairChanged(pairId);
                return;
            }
        }
    }

    void PairController::addPair(const PairCandidate& candidate)
    {
        Pair pair;
        pair.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        pair.localPath = candidate.localPath;
        pair.remotePath = candidate.remotePath;
        pair.remoteHandle = candidate.remoteHandle;

        rescanPair(pair);

        mQueue.pairs.append(pair);
        persist();
        emit pairAdded(pair.id);
    }

    void PairController::removePair(const QString& pairId)
    {
        for (int i = 0; i < mQueue.pairs.size(); ++i)
        {
            if (mQueue.pairs.at(i).id == pairId)
            {
                mQueue.pairs.removeAt(i);
                mClassifications.remove(pairId);
                mReFlagged.remove(pairId);
                persist();
                emit pairRemoved(pairId);
                return;
            }
        }
    }

    bool PairController::applyPlan(const QString& pairId)
    {
        Pair* pairPtr = nullptr;
        for (Pair& pair : mQueue.pairs)
        {
            if (pair.id == pairId)
            {
                pairPtr = &pair;
                break;
            }
        }
        if (!pairPtr)
        {
            return false;
        }
        if (!mPlanApplier)
        {
            mLastError = QStringLiteral("No plan applier installed");
            return false;
        }

        const Plan pairPlan = plan(pairId);
        if (!mPlanApplier(pairId, pairPlan))
        {
            mLastError = QStringLiteral("Could not apply the plan for pair %1").arg(pairPtr->localPath);
            return false;
        }

        // Re-scan the pair against the applied data and re-verify the
        // decisions (Reconciler): rows the plan made vanish lose their
        // decisions, rows whose classification changed are re-flagged.
        if (rescanPair(*pairPtr))
        {
            Queue single;
            single.pairs.append(*pairPtr);
            QHash<QString, Classification> fresh;
            fresh.insert(pairId, mClassifications.value(pairId));

            const ReconcileResult result = Reconciler().reconcile(single, fresh);
            *pairPtr = result.queue.pairs.first();
            mReFlagged.insert(pairId, result.reFlaggedPaths.value(pairId));
        }

        persist();
        emit pairChanged(pairId);
        return true;
    }

    bool PairController::commitPair(const QString& pairId)
    {
        if (!allApproved(pairId))
        {
            mLastError = QStringLiteral("Not every row is approved yet");
            return false;
        }

        // The mocked commit applies the plan to the pair's fake data (the
        // re-added scenario then re-scans to the post-commit state) before
        // the pair leaves the queue. Without an applier the drop still
        // stands in for sync creation.
        if (mPlanApplier)
        {
            const Plan pairPlan = plan(pairId);
            if (!mPlanApplier(pairId, pairPlan))
            {
                mLastError = QStringLiteral("Could not apply the plan for pair %1").arg(pairId);
                return false;
            }
        }

        removePair(pairId);
        return true;
    }

    int PairController::awaitingApprovalCount(const QString& pairId) const
    {
        return awaitingApprovalPaths(pairId).size();
    }

    QStringList PairController::awaitingApprovalPaths(const QString& pairId) const
    {
        QStringList awaiting;
        const Classification& classification = this->classification(pairId);
        const Pair* pairPtr = pair(pairId);
        if (!pairPtr)
        {
            return awaiting;
        }

        QHash<QString, Action> decisions;
        QHash<QString, bool> approvals;
        for (const RowDecision& decision : pairPtr->decisions)
        {
            decisions.insert(decision.relativePath, decision.action);
            approvals.insert(decision.relativePath, decision.approved);
        }

        for (const Row& row : classification.rows)
        {
            if (!row.requiresApproval || approvals.value(row.relativePath, false))
            {
                continue;
            }

            // Effective action: own decision > nearest ancestor directory
            // decision > recommended action. Only an EXPLICIT do-nothing
            // decision (own or inherited) takes a flagged row out of the
            // gate; an undecided row whose recommendation is None (conflict
            // and blocker rows) still awaits a decision.
            Action effective = row.recommendedAction;
            bool explicitDecision = decisions.contains(row.relativePath);
            if (explicitDecision)
            {
                effective = decisions.value(row.relativePath);
            }
            else
            {
                QString ancestor = parentPath(row.relativePath);
                while (!ancestor.isEmpty())
                {
                    if (decisions.contains(ancestor))
                    {
                        effective = decisions.value(ancestor);
                        explicitDecision = true;
                        break;
                    }
                    ancestor = parentPath(ancestor);
                }
            }

            if (explicitDecision && effective == Action::None)
            {
                continue;
            }
            awaiting.append(row.relativePath);
        }
        return awaiting;
    }

    bool PairController::allApproved(const QString& pairId) const
    {
        return awaitingApprovalCount(pairId) == 0;
    }

    bool PairController::rescanPair(Pair& pair)
    {
        mLastError.clear();
        if (!mSideProviderFactory)
        {
            return false;
        }

        const auto providers = mSideProviderFactory(pair);
        if (!providers)
        {
            mLastError = QStringLiteral("No provider for pair %1").arg(pair.localPath);
            return false;
        }

        const Classifier classifier;
        mClassifications.insert(pair.id, classifier.classify(*providers->local, *providers->remote));
        return true;
    }

    void PairController::persist()
    {
        if (!mStore.save(mQueue))
        {
            mLastError = QStringLiteral("Could not write %1").arg(mStore.filePath());
        }
    }
}