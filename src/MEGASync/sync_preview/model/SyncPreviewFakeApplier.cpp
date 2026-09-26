#include "SyncPreviewFakeApplier.h"

#include <QPair>
#include <QSet>
#include <QStringList>
#include <QVector>

#include <algorithm>

namespace SyncPreview
{
    namespace
    {
        // Working representation of one side: entries keyed by path, so an
        // operation sequence that a real filesystem could only execute via
        // intermediate moves (the rename-aware swap) resolves naturally —
        // entries move as whole values and the trees are rebuilt at the end.
        struct WorkingSide
        {
            QHash<QString, Entry> entries;

            bool contains(const QString& path) const { return entries.contains(path); }

            void ensureAncestors(const QString& relativePath)
            {
                const QStringList parts = splitPath(relativePath);
                QString current;
                for (int i = 0; i < parts.size() - 1; ++i)
                {
                    current = appendPath(current, parts.at(i));
                    if (!entries.contains(current))
                    {
                        Entry folder;
                        folder.relativePath = current;
                        folder.type = EntryType::Folder;
                        entries.insert(current, folder);
                    }
                }
            }

            void place(const Entry& entry)
            {
                ensureAncestors(entry.relativePath);
                entries.insert(entry.relativePath, entry);
            }

            void removeSubtree(const QString& path)
            {
                entries.remove(path);
                const QString prefix = path + QLatin1Char('/');
                QStringList doomed;
                for (auto it = entries.constBegin(); it != entries.constEnd(); ++it)
                {
                    if (it.key().startsWith(prefix))
                    {
                        doomed.append(it.key());
                    }
                }
                for (const QString& key : doomed)
                {
                    entries.remove(key);
                }
            }

            // One rename applied on its own: the entry at `from` (and its
            // subtree, for folders) moves to `to`; `to`'s occupant, if any,
            // is overwritten (the planner never emits a colliding single
            // rename — occupied targets only arise in same-row swap pairs,
            // which are batched atomically by the caller).
            void renameEntry(const QString& from, const QString& to)
            {
                if (!entries.contains(from))
                {
                    return;
                }
                removeSubtree(to);

                const Entry moving = entries.take(from);
                ensureAncestors(to);
                Entry moved = moving;
                moved.relativePath = to;
                entries.insert(to, moved);

                if (!moving.isFolder())
                {
                    return;
                }
                const QString prefix = from + QLatin1Char('/');
                QVector<QPair<QString, Entry>> relocations;
                QStringList doomed;
                for (auto it = entries.constBegin(); it != entries.constEnd(); ++it)
                {
                    if (it.key().startsWith(prefix))
                    {
                        Entry child = it.value();
                        child.relativePath = to + it.key().mid(from.size());
                        relocations.append(qMakePair(child.relativePath, child));
                        doomed.append(it.key());
                    }
                }
                for (const QString& key : doomed)
                {
                    entries.remove(key);
                }
                for (const auto& relocation : relocations)
                {
                    entries.insert(relocation.first, relocation.second);
                }
            }

            // A same-row group of rename operations applied as one atomic
            // permutation (the rename-aware swap): every affected entry
            // moves directly to its mapping target, so mutual targets
            // resolve without losing either copy. Subtree children move
            // with their renamed folder.
            void applyRenameBatch(const QVector<QPair<QString, QString>>& mappings)
            {
                if (mappings.isEmpty())
                {
                    return;
                }

                QVector<QPair<QString, Entry>> relocations;
                QStringList doomed;
                for (const auto& mapping : mappings)
                {
                    const QString from = mapping.first;
                    const QString to = mapping.second;
                    const auto entryIt = entries.constFind(from);
                    if (entryIt == entries.constEnd())
                    {
                        continue;
                    }
                    doomed.append(from);
                    relocations.append(qMakePair(to, entryIt.value()));
                    if (!entryIt.value().isFolder())
                    {
                        continue;
                    }
                    const QString prefix = from + QLatin1Char('/');
                    for (auto it = entries.constBegin(); it != entries.constEnd(); ++it)
                    {
                        if (it.key().startsWith(prefix))
                        {
                            doomed.append(it.key());
                            Entry child = it.value();
                            child.relativePath = to + it.key().mid(from.size());
                            relocations.append(qMakePair(child.relativePath, child));
                        }
                    }
                }

                for (const QString& key : doomed)
                {
                    entries.remove(key);
                }
                for (const auto& relocation : relocations)
                {
                    ensureAncestors(relocation.first);
                    Entry moved = relocation.second;
                    moved.relativePath = relocation.first;
                    entries.insert(relocation.first, moved);
                }
            }

            Tree build() const
            {
                // Deterministic parents-first order: by depth, then
                // case-insensitive path.
                QVector<Entry> ordered;
                ordered.reserve(entries.size());
                for (auto it = entries.constBegin(); it != entries.constEnd(); ++it)
                {
                    ordered.append(it.value());
                }
                std::sort(ordered.begin(), ordered.end(),
                    [](const Entry& a, const Entry& b)
                    {
                        const int depthA = a.relativePath.count(QLatin1Char('/'));
                        const int depthB = b.relativePath.count(QLatin1Char('/'));
                        if (depthA != depthB)
                        {
                            return depthA < depthB;
                        }
                        return a.relativePath.compare(b.relativePath, Qt::CaseInsensitive) < 0;
                    });

                Tree tree;
                for (const Entry& entry : ordered)
                {
                    tree.addEntry(entry);
                }
                return tree;
            }
        };

        bool isRenameOp(OperationType type)
        {
            return type == OperationType::RenameRemote || type == OperationType::RenameLocal;
        }

        // Transfers and replaces share one application: the destination
        // side ends up holding the source-side entry data at the path
        // (missing ancestor folders created, existing entries
        // overwritten). Folder subtree ops bring the descendants along.
        void transferEntry(WorkingSide& source, WorkingSide& dest, const QString& path, bool isFolder, bool isSubtree)
        {
            const auto sourceIt = source.entries.constFind(path);
            if (sourceIt == source.entries.constEnd())
            {
                return;
            }
            dest.place(sourceIt.value());
            if (!isFolder || !isSubtree)
            {
                return;
            }
            const QString prefix = path + QLatin1Char('/');
            for (auto it = source.entries.constBegin(); it != source.entries.constEnd(); ++it)
            {
                if (it.key().startsWith(prefix))
                {
                    dest.place(it.value());
                }
            }
        }

        void applyOperation(WorkingSide& local, WorkingSide& remote, const PlannedOperation& operation)
        {
            switch (operation.type)
            {
                case OperationType::Upload:
                case OperationType::UploadReplace:
                    transferEntry(local, remote, operation.path, operation.isFolder, operation.isSubtree);
                    break;
                case OperationType::Download:
                case OperationType::DownloadReplace:
                    transferEntry(remote, local, operation.path, operation.isFolder, operation.isSubtree);
                    break;
                case OperationType::DeleteRemoteToRubbish:
                    remote.removeSubtree(operation.path);
                    break;
                case OperationType::DeleteLocalToTrash:
                    local.removeSubtree(operation.path);
                    break;
                case OperationType::RenameRemote:
                    remote.renameEntry(operation.fromPath, operation.toPath);
                    break;
                case OperationType::RenameLocal:
                    local.renameEntry(operation.fromPath, operation.toPath);
                    break;
                case OperationType::None:
                    break;
            }
        }
    }

    void applyPlan(FakeScenario& scenario, const Plan& plan)
    {
        WorkingSide local;
        for (const Entry& entry : scenario.local.entries())
        {
            local.entries.insert(entry.relativePath, entry);
        }
        WorkingSide remote;
        for (const Entry& entry : scenario.remote.entries())
        {
            remote.entries.insert(entry.relativePath, entry);
        }

        for (const RowPlan& rowPlan : plan.rows)
        {
            if (rowPlan.operations.isEmpty())
            {
                continue;
            }

            // Same-row renames that touch each other's paths form one
            // exchange (the rename-aware swap): applied as a single atomic
            // permutation so neither copy is lost. Everything else applies
            // in plan order.
            QSet<QString> swapPaths;
            bool overlapping = false;
            for (const PlannedOperation& operation : rowPlan.operations)
            {
                if (!isRenameOp(operation.type))
                {
                    continue;
                }
                if (swapPaths.contains(operation.fromPath) || swapPaths.contains(operation.toPath))
                {
                    overlapping = true;
                }
                swapPaths.insert(operation.fromPath);
                swapPaths.insert(operation.toPath);
            }

            if (overlapping)
            {
                QVector<QPair<QString, QString>> localMappings;
                QVector<QPair<QString, QString>> remoteMappings;
                for (const PlannedOperation& operation : rowPlan.operations)
                {
                    if (operation.type == OperationType::RenameLocal)
                    {
                        localMappings.append(qMakePair(operation.fromPath, operation.toPath));
                    }
                    else if (operation.type == OperationType::RenameRemote)
                    {
                        remoteMappings.append(qMakePair(operation.fromPath, operation.toPath));
                    }
                    else
                    {
                        applyOperation(local, remote, operation);
                    }
                }
                local.applyRenameBatch(localMappings);
                remote.applyRenameBatch(remoteMappings);
            }
            else
            {
                for (const PlannedOperation& operation : rowPlan.operations)
                {
                    applyOperation(local, remote, operation);
                }
            }
        }

        scenario.local = local.build();
        scenario.remote = remote.build();
    }
}