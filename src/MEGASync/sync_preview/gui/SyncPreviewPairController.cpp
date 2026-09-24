#include "SyncPreviewPairController.h"
#include "SyncPreviewQueueStore.h"
#include "SyncPreviewReconciler.h"

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

        const auto classificationIt = mClassifications.constFind(pairId);
        const Row* row = classificationIt != mClassifications.constEnd()
            ? classificationIt->find(relativePath) : nullptr;

        for (RowDecision& decision : pairPtr->decisions)
        {
            if (decision.relativePath == relativePath)
            {
                decision.approved = approved;
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

        RowDecision decision;
        decision.relativePath = relativePath;
        decision.action = row ? row->recommendedAction : Action::None;
        decision.approved = approved;
        decision.kind = row ? static_cast<int>(row->kind) : RowDecision::UNKNOWN_KIND;
        decision.requiresApproval = row ? row->requiresApproval : false;
        pairPtr->decisions.append(decision);
        persist();
        emit pairChanged(pairId);
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

    int PairController::awaitingApprovalCount(const QString& pairId) const
    {
        const Classification& classification = this->classification(pairId);
        const Pair* pairPtr = pair(pairId);
        if (!pairPtr)
        {
            return 0;
        }

        QHash<QString, bool> approvals;
        for (const RowDecision& decision : pairPtr->decisions)
        {
            approvals.insert(decision.relativePath, decision.approved);
        }

        int count = 0;
        for (const Row& row : classification.rows)
        {
            if (row.requiresApproval && !approvals.value(row.relativePath, false))
            {
                ++count;
            }
        }
        return count;
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