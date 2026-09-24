#include "SyncPreviewClassifier.h"

#include <QHash>
#include <QSet>

namespace SyncPreview
{
    namespace
    {
        // True when the whole subtree rooted at `folderPath` exists on both
        // sides with identical types and content. Exact-path pairing only:
        // case-insensitive collisions and same-content-different-name twins
        // make a subtree non-identical, which is the honest state for the
        // enclosing directory row.
        bool subtreeIdentical(const Tree& localTree,
                              const Tree& remoteTree,
                              const QHash<QString, int>& remoteExact,
                              const QString& folderPath,
                              QHash<QString, bool>& memo)
        {
            const auto it = memo.constFind(folderPath);
            if (it != memo.constEnd())
            {
                return it.value();
            }

            bool identical = true;
            for (const Entry& localChild : localTree.entries())
            {
                if (parentPath(localChild.relativePath) != folderPath)
                {
                    continue;
                }

                const auto remoteIt = remoteExact.constFind(localChild.relativePath);
                if (remoteIt == remoteExact.constEnd())
                {
                    identical = false;
                    break;
                }

                const Entry& remoteChild = remoteTree.entries().at(remoteIt.value());
                if (localChild.type != remoteChild.type)
                {
                    identical = false;
                    break;
                }

                if (localChild.isFolder())
                {
                    if (!subtreeIdentical(localTree, remoteTree, remoteExact, localChild.relativePath, memo))
                    {
                        identical = false;
                        break;
                    }
                }
                else if (!sameContent(localChild, remoteChild))
                {
                    identical = false;
                    break;
                }
            }

            if (identical)
            {
                // A folder missing on one side under this folder also breaks
                // identity; local-only children were caught above (their
                // exact remote match is missing). Remote-only children must
                // be caught here.
                for (const Entry& remoteChild : remoteTree.entries())
                {
                    if (parentPath(remoteChild.relativePath) == folderPath && !localTree.contains(remoteChild.relativePath))
                    {
                        identical = false;
                        break;
                    }
                }
            }

            memo.insert(folderPath, identical);
            return identical;
        }

        Row makePairRow(const Entry& localEntry, const Entry& remoteEntry, bool subtreeSame)
        {
            Row row;
            row.relativePath = localEntry.relativePath;
            row.local = localEntry;
            row.remote = remoteEntry;

            if (localEntry.type != remoteEntry.type)
            {
                row.kind = RowKind::Blocker;
                row.blockerReason = BlockerReason::TypeMismatch;
                row.requiresApproval = true;
                return row;
            }

            if (localEntry.isFolder())
            {
                row.kind = subtreeSame ? RowKind::Identical : RowKind::BothDiffer;
            }
            else
            {
                row.kind = sameContent(localEntry, remoteEntry) ? RowKind::Identical : RowKind::BothDiffer;
            }

            row.requiresApproval = (row.kind == RowKind::BothDiffer);
            return row;
        }

        Action recommendedActionFor(const Row& row)
        {
            switch (row.kind)
            {
                case RowKind::LocalOnly:
                    return Action::LocalToRemote;
                case RowKind::RemoteOnly:
                    return Action::RemoteToLocal;
                case RowKind::Identical:
                    return Action::None;
                case RowKind::BothDiffer:
                    // Newer modification time wins; a tie recommends L->R
                    // (deterministic tie-break, always overridable).
                    if (row.local && row.remote && row.local->modifiedTime < row.remote->modifiedTime)
                    {
                        return Action::RemoteToLocal;
                    }
                    return Action::LocalToRemote;
                case RowKind::Conflict:
                case RowKind::Blocker:
                    // No default: these need an explicit user decision.
                    return Action::None;
            }
            return Action::None;
        }
    }

    const Row* Classification::find(const QString& relativePath) const
    {
        for (const Row& row : rows)
        {
            if (row.relativePath == relativePath)
            {
                return &row;
            }
        }
        return nullptr;
    }

    Classification Classifier::classify(const LocalSideProvider& localSide, const RemoteSideProvider& remoteSide) const
    {
        const Tree localTree = localSide.snapshot();
        const Tree remoteTree = remoteSide.snapshot();
        Classification result;

        QHash<QString, int> remoteExact;
        for (int i = 0; i < remoteTree.entries().size(); ++i)
        {
            remoteExact.insert(remoteTree.entries().at(i).relativePath, i);
        }

        QVector<int> remoteConsumed(remoteTree.entries().size(), 0);

        // Pass A: exact-path pairing (and local-only placeholders), in local
        // tree order so the row order is stable for the UI.
        QHash<QString, bool> subtreeMemo;
        QVector<int> localPlaceholderRows;
        for (const Entry& localEntry : localTree.entries())
        {
            const auto remoteIt = remoteExact.constFind(localEntry.relativePath);
            if (remoteIt != remoteExact.constEnd())
            {
                const bool subtreeSame = localEntry.isFolder()
                    ? subtreeIdentical(localTree, remoteTree, remoteExact, localEntry.relativePath, subtreeMemo)
                    : false;
                result.rows.append(makePairRow(localEntry, remoteTree.entries().at(remoteIt.value()), subtreeSame));
                remoteConsumed[remoteIt.value()] = 1;
            }
            else
            {
                Row placeholder;
                placeholder.relativePath = localEntry.relativePath;
                placeholder.local = localEntry;
                localPlaceholderRows.append(result.rows.size());
                result.rows.append(placeholder);
            }
        }

        // Pass B: case-insensitive name collisions between leftovers.
        // Equal-insensitive spellings cannot coexist on one filesystem slot,
        // so the two entries merge into one blocker row (types differing at
        // a case-insensitive match is a type mismatch, not a case collision).
        QHash<int, QString> remoteTwinOf;
        for (const int rowIndex : localPlaceholderRows)
        {
            Row& row = result.rows[rowIndex];
            if (row.kind != RowKind::LocalOnly)
            {
                continue;
            }
            const Entry& localEntry = *row.local;
            const QString key = caseInsensitiveKey(localEntry.relativePath);

            for (int j = 0; j < remoteTree.entries().size(); ++j)
            {
                if (remoteConsumed[j])
                {
                    continue;
                }
                const Entry& remoteEntry = remoteTree.entries().at(j);
                if (caseInsensitiveKey(remoteEntry.relativePath) != key)
                {
                    continue;
                }

                row.remote = remoteEntry;
                row.kind = RowKind::Blocker;
                row.requiresApproval = true;
                row.blockerReason = (localEntry.type != remoteEntry.type) ? BlockerReason::TypeMismatch
                                                                          : BlockerReason::CaseInsensitiveNameCollision;
                remoteConsumed[j] = 1;
                break;
            }
        }

        // Pass C: same-content-different-name twins. Both sides stay
        // separate, independently decidable rows (user decision), each
        // flagged with the advisory twin path; the remote twin row is
        // emitted in the remote-leftover pass.
        for (const int rowIndex : localPlaceholderRows)
        {
            Row& row = result.rows[rowIndex];
            if (row.kind != RowKind::LocalOnly || row.local->isFolder())
            {
                continue;
            }

            for (int j = 0; j < remoteTree.entries().size(); ++j)
            {
                if (remoteConsumed[j])
                {
                    continue;
                }
                const Entry& remoteEntry = remoteTree.entries().at(j);
                if (remoteEntry.isFolder() || !sameContent(*row.local, remoteEntry))
                {
                    continue;
                }

                row.kind = RowKind::Conflict;
                row.hasIdenticalTwin = true;
                row.twinPath = remoteEntry.relativePath;
                row.requiresApproval = true;
                remoteTwinOf.insert(j, row.relativePath);
                remoteConsumed[j] = 1;
                break;
            }
        }

        // Remote-only leftovers (and the remote side of twins), in remote
        // tree order.
        for (int j = 0; j < remoteTree.entries().size(); ++j)
        {
            const bool twinMapped = remoteTwinOf.contains(j);
            if (remoteConsumed[j] && !twinMapped)
            {
                continue;
            }

            const Entry& remoteEntry = remoteTree.entries().at(j);
            Row row;
            row.relativePath = remoteEntry.relativePath;
            row.remote = remoteEntry;
            if (twinMapped)
            {
                row.kind = RowKind::Conflict;
                row.hasIdenticalTwin = true;
                row.twinPath = remoteTwinOf.value(j);
                row.requiresApproval = true;
            }
            else
            {
                row.kind = RowKind::RemoteOnly;
            }
            result.rows.append(row);
        }

        // Advisory: entries under a type-mismatch blocker path.
        for (const Row& blocker : result.rows)
        {
            if (blocker.kind != RowKind::Blocker || blocker.blockerReason != BlockerReason::TypeMismatch)
            {
                continue;
            }

            QString blockedPrefix;
            if (blocker.local && blocker.local->isFolder())
            {
                blockedPrefix = blocker.local->relativePath;
            }
            else if (blocker.remote && blocker.remote->isFolder())
            {
                blockedPrefix = blocker.remote->relativePath;
            }

            if (blockedPrefix.isEmpty())
            {
                continue;
            }
            blockedPrefix += QLatin1Char('/');

            for (Row& row : result.rows)
            {
                if (&row != &blocker && row.relativePath.startsWith(blockedPrefix))
                {
                    row.underBlockedPath = true;
                    row.requiresApproval = true;
                }
            }
        }

        for (Row& row : result.rows)
        {
            row.recommendedAction = recommendedActionFor(row);
        }

        return result;
    }
}
