#include "SyncPreviewPlanner.h"

#include <QSet>
#include <QVector>

namespace SyncPreview
{
    namespace
    {
        struct Effective
        {
            Action action = Action::None;
            ActionSource source = ActionSource::Recommended;
            QString sourcePath;
        };

        // Shape of the operation a single-sided directory row emits.
        struct DirOpShape
        {
            OperationType type = OperationType::None;
            bool subtree = false;  // false: node-only (folder creation / placement)
        };

        bool isFileRow(const Row& row)
        {
            if (row.kind == RowKind::Blocker)
            {
                return false;
            }
            if (row.local && row.local->type == EntryType::File)
            {
                return true;
            }
            if (row.remote && row.remote->type == EntryType::File)
            {
                return true;
            }
            return false;
        }

        bool isDirRow(const Row& row)
        {
            if (row.kind == RowKind::Blocker || row.kind == RowKind::Conflict)
            {
                return false;
            }
            if (row.local && row.local->isFolder())
            {
                return true;
            }
            if (row.remote && row.remote->isFolder())
            {
                return true;
            }
            return false;
        }

        bool isSingleSidedDirRow(const Row& row)
        {
            return (row.kind == RowKind::LocalOnly || row.kind == RowKind::RemoteOnly) && isDirRow(row);
        }

        bool isDeletion(OperationType type)
        {
            return type == OperationType::DeleteRemoteToRubbish || type == OperationType::DeleteLocalToTrash;
        }

        bool isRename(OperationType type)
        {
            return type == OperationType::RenameRemote || type == OperationType::RenameLocal;
        }

        void addConsequence(RowPlan& plan, const PlannedOperation& operation)
        {
            switch (operation.type)
            {
                case OperationType::Upload:
                    plan.createdRemote.append(operation.path);
                    break;
                case OperationType::Download:
                    plan.createdLocal.append(operation.path);
                    break;
                case OperationType::UploadReplace:
                    plan.changedRemote.append(operation.path);
                    break;
                case OperationType::DownloadReplace:
                    plan.changedLocal.append(operation.path);
                    break;
                case OperationType::DeleteRemoteToRubbish:
                    plan.removedRemote.append(operation.path);
                    break;
                case OperationType::DeleteLocalToTrash:
                    plan.removedLocal.append(operation.path);
                    break;
                case OperationType::RenameRemote:
                    plan.renamedRemote.append(qMakePair(operation.fromPath, operation.toPath));
                    break;
                case OperationType::RenameLocal:
                    plan.renamedLocal.append(qMakePair(operation.fromPath, operation.toPath));
                    break;
                case OperationType::None:
                    break;
            }
        }

        PlannedOperation plannedOperation(OperationType type, const QString& path, bool isFolder = false, bool isSubtree = false)
        {
            PlannedOperation operation;
            operation.type = type;
            operation.path = path;
            operation.isFolder = isFolder;
            operation.isSubtree = isSubtree;
            return operation;
        }

        PlannedOperation renamePlannedOperation(OperationType type, const QString& rowPath,
                                                const QString& fromPath, const QString& toPath, bool isFolder)
        {
            PlannedOperation operation;
            operation.type = type;
            operation.path = rowPath;
            operation.isFolder = isFolder;
            operation.fromPath = fromPath;
            operation.toPath = toPath;
            return operation;
        }

        // True when the entry's content also exists somewhere else on the
        // given side (any other row's entry on that side), i.e. overwriting
        // or removing it loses no last copy.
        bool contentPreservedOnSide(const Classification& classification, const Entry& entry, bool localSide,
                                    const QString& excludeRowPath)
        {
            for (const Row& other : classification.rows)
            {
                if (other.relativePath == excludeRowPath)
                {
                    continue;
                }
                const std::optional<Entry>& candidate = localSide ? other.local : other.remote;
                if (candidate && sameContent(*candidate, entry))
                {
                    return true;
                }
            }
            return false;
        }

        // First free "name (N)[.ext]" spelling for an entry about to be
        // displaced by an automatic rename (MEGA-2.9). Uniqueness is
        // case-insensitive against every sibling path the destination side
        // already holds, mirroring the case-insensitivity that makes a
        // blocker out of same-name entries. Folders never split an
        // extension off their name.
        QString freeDisplacedName(const Classification& classification, bool localSide, const QString& existingPath, bool isFolder)
        {
            const QString dir = parentPath(existingPath);
            const QString base = existingPath.mid(dir.isEmpty() ? 0 : dir.size() + 1);

            QString stem = base;
            QString extension;
            if (!isFolder)
            {
                const int dot = base.lastIndexOf(QLatin1Char('.'));
                if (dot > 0)
                {
                    stem = base.left(dot);
                    extension = base.mid(dot);
                }
            }

            QSet<QString> occupied;
            for (const Row& row : classification.rows)
            {
                const std::optional<Entry>& side = localSide ? row.local : row.remote;
                if (!side || parentPath(side->relativePath) != dir)
                {
                    continue;
                }
                occupied.insert(caseInsensitiveKey(side->relativePath));
            }

            for (int n = 1; n <= 1000; ++n)
            {
                const QString candidate =
                    appendPath(dir, QStringLiteral("%1 (%2)%3").arg(stem).arg(n).arg(extension));
                if (!occupied.contains(caseInsensitiveKey(candidate)))
                {
                    return candidate;
                }
            }
            return appendPath(dir, stem + QStringLiteral(" (renamed)") + extension);
        }

        // Operation list for one file row from its effective action. The
        // rename-aware expansions (MEGA-2.9): on conflict rows a transfer
        // adopts the identical-content twin by renaming it to the row's
        // path instead of duplicating content; on paired rows with a twin
        // the transfer becomes the rename-aware swap so no content is lost;
        // duplicate/unique-content advisory warnings are appended.
        QVector<PlannedOperation> operationsForFileRow(const Row& row, Action action,
                                                       const Classification& classification, QStringList& warnings)
        {
            QVector<PlannedOperation> operations;
            if (action == Action::None)
            {
                return operations;
            }

            const bool hasLocal = row.local.has_value();
            const bool hasRemote = row.remote.has_value();

            if (hasLocal && !hasRemote)
            {
                if (action == Action::LocalToRemote && row.kind == RowKind::Conflict && !row.twinPath.isEmpty())
                {
                    // Adopt: the identical content already sits on the
                    // remote side under the twin's name; renaming it to this
                    // row's name resolves the conflict without a duplicate.
                    operations.append(renamePlannedOperation(OperationType::RenameRemote, row.relativePath,
                                                             row.twinPath, row.relativePath, false));
                    warnings.append(QStringLiteral("Transfers by rename: the identical remote copy at %1 is renamed to %2, no content is uploaded")
                                        .arg(row.twinPath, row.relativePath));
                    return operations;
                }

                switch (action)
                {
                    case Action::LocalToRemote:
                    case Action::BestEffort:
                        operations.append(plannedOperation(OperationType::Upload, row.relativePath));
                        break;
                    case Action::RemoteToLocal:
                        operations.append(plannedOperation(OperationType::DeleteLocalToTrash, row.relativePath));
                        break;
                    case Action::None:
                        break;
                }
                if (!operations.isEmpty() && operations.first().type == OperationType::Upload && !row.twinPath.isEmpty())
                {
                    warnings.append(QStringLiteral("Uploading %1 duplicates identical content already on the remote side at %2")
                                        .arg(row.relativePath, row.twinPath));
                }
            }
            else if (!hasLocal && hasRemote)
            {
                if (action == Action::RemoteToLocal && row.kind == RowKind::Conflict && !row.twinPath.isEmpty())
                {
                    operations.append(renamePlannedOperation(OperationType::RenameLocal, row.relativePath,
                                                             row.twinPath, row.relativePath, false));
                    warnings.append(QStringLiteral("Transfers by rename: the identical local copy at %1 is renamed to %2, no content is downloaded")
                                        .arg(row.twinPath, row.relativePath));
                    return operations;
                }

                switch (action)
                {
                    case Action::LocalToRemote:
                        operations.append(plannedOperation(OperationType::DeleteRemoteToRubbish, row.relativePath));
                        break;
                    case Action::RemoteToLocal:
                    case Action::BestEffort:
                        operations.append(plannedOperation(OperationType::Download, row.relativePath));
                        break;
                    case Action::None:
                        break;
                }
                if (operations.first().type == OperationType::Download && !row.twinPath.isEmpty())
                {
                    warnings.append(QStringLiteral("Downloading %1 duplicates identical content already on the local side at %2")
                                        .arg(row.relativePath, row.twinPath));
                }
            }
            else
            {
                // Both sides present, differing (or a blocker row; blockers
                // are handled separately before this function).
                if (action == Action::LocalToRemote && row.kind == RowKind::BothDiffer && !row.twinPath.isEmpty())
                {
                    // Rename-aware swap (MEGA-2.9): the local content
                    // already exists on the remote side at the twin path, so
                    // uploading would overwrite content the remote already
                    // keeps elsewhere. First displace the remote entry at
                    // this path (renamed into the twin's vacated name, or to
                    // Rubbish when its content survives elsewhere on the
                    // remote side), then adopt the twin at the row's path.
                    if (contentPreservedOnSide(classification, *row.remote, false, row.relativePath))
                    {
                        operations.append(plannedOperation(OperationType::DeleteRemoteToRubbish, row.relativePath));
                    }
                    else
                    {
                        operations.append(renamePlannedOperation(OperationType::RenameRemote, row.relativePath,
                                                                 row.relativePath, row.twinPath, false));
                    }
                    operations.append(renamePlannedOperation(OperationType::RenameRemote, row.relativePath,
                                                             row.twinPath, row.relativePath, false));
                    warnings.append(QStringLiteral("Rename-aware swap on the remote side: %1 and %2 exchange names so both contents survive")
                                        .arg(row.relativePath, row.twinPath));
                    return operations;
                }
                if (action == Action::RemoteToLocal && row.kind == RowKind::BothDiffer && !row.localTwinPath.isEmpty())
                {
                    if (contentPreservedOnSide(classification, *row.local, true, row.relativePath))
                    {
                        operations.append(plannedOperation(OperationType::DeleteLocalToTrash, row.relativePath));
                    }
                    else
                    {
                        operations.append(renamePlannedOperation(OperationType::RenameLocal, row.relativePath,
                                                                 row.relativePath, row.localTwinPath, false));
                    }
                    operations.append(renamePlannedOperation(OperationType::RenameLocal, row.relativePath,
                                                             row.localTwinPath, row.relativePath, false));
                    warnings.append(QStringLiteral("Rename-aware swap on the local side: %1 and %2 exchange names so both contents survive")
                                        .arg(row.relativePath, row.localTwinPath));
                    return operations;
                }

                switch (action)
                {
                    case Action::LocalToRemote:
                        operations.append(plannedOperation(OperationType::UploadReplace, row.relativePath));
                        if (!contentPreservedOnSide(classification, *row.remote, false, row.relativePath))
                        {
                            warnings.append(QStringLiteral("Replacing %1 on the remote side discards content that is not preserved anywhere else on the remote side (recoverable via MEGA Rubbish)")
                                                .arg(row.relativePath));
                        }
                        break;
                    case Action::RemoteToLocal:
                        operations.append(plannedOperation(OperationType::DownloadReplace, row.relativePath));
                        if (!contentPreservedOnSide(classification, *row.local, true, row.relativePath))
                        {
                            warnings.append(QStringLiteral("Replacing %1 on the local side discards content that is not preserved anywhere else on the local side (recoverable via the OS trash)")
                                                .arg(row.relativePath));
                        }
                        break;
                    case Action::BestEffort:
                        warnings.append(QStringLiteral("Best-effort cannot merge differing content of %1; flagged as conflict, no transfer planned")
                                            .arg(row.relativePath));
                        break;
                    case Action::None:
                        break;
                }
            }

            return operations;
        }

        // Blocker rows cannot transfer onto the blocked path: the arrow
        // actions mean "transfer with automatic rename" (MEGA-2.9) — the
        // destination-side entry occupying the path is displaced by an
        // automatic rename (first free "name (N)[.ext]", case-insensitive),
        // then the transfer lands on the freed path.
        QVector<PlannedOperation> operationsForBlockerRow(const Row& row, Action action,
                                                          const Classification& classification, QStringList& warnings)
        {
            QVector<PlannedOperation> operations;
            if (action != Action::LocalToRemote && action != Action::RemoteToLocal)
            {
                if (action == Action::BestEffort)
                {
                    warnings.append(QStringLiteral("Best-effort cannot resolve a blocked row; it needs a transfer with automatic rename or exclusion"));
                }
                return operations;
            }

            const bool toRemote = action == Action::LocalToRemote;
            const Entry& displaced = toRemote ? *row.remote : *row.local;
            const Entry& transferred = toRemote ? *row.local : *row.remote;
            const QString displacedPath = displaced.relativePath;
            // The transfer lands at the destination side's own spelling: the
            // local spelling for L->R uploads, the remote spelling for R->L
            // downloads (case-collision spellings differ between the sides).
            const QString transferPath = toRemote ? row.relativePath
                                                  : (row.remote ? row.remote->relativePath : row.relativePath);
            const QString displacedName =
                freeDisplacedName(classification, !toRemote, displacedPath, displaced.isFolder());

            PlannedOperation rename = renamePlannedOperation(
                toRemote ? OperationType::RenameRemote : OperationType::RenameLocal,
                row.relativePath, displacedPath, displacedName, displaced.isFolder());
            operations.append(rename);
            operations.append(plannedOperation(toRemote ? OperationType::Upload : OperationType::Download,
                                               transferPath, transferred.isFolder()));

            warnings.append(QStringLiteral("Blocked row resolved with an automatic rename: %1 %2 renamed to %3, then %4")
                                .arg(toRemote ? QStringLiteral("remote") : QStringLiteral("local"),
                                     displacedPath, displacedName,
                                     toRemote ? QStringLiteral("the local content is uploaded") : QStringLiteral("the remote content is downloaded")));
            return operations;
        }

        // Operation shape a single-sided directory row emits for its
        // effective action: deletions are always subtree ops; transfers are
        // subtree ops when the directory's descendants all follow the
        // cascade, and node-only ops when some descendant carries an
        // explicit decision of its own.
        DirOpShape dirOpShapeFor(const Row& row, Action action, bool hasExplicitDescendant)
        {
            DirOpShape shape;
            if (action == Action::None || !isSingleSidedDirRow(row))
            {
                return shape;
            }

            if (row.kind == RowKind::LocalOnly)
            {
                switch (action)
                {
                    case Action::LocalToRemote:
                    case Action::BestEffort:
                        shape.type = OperationType::Upload;
                        shape.subtree = !hasExplicitDescendant;
                        break;
                    case Action::RemoteToLocal:
                        shape.type = OperationType::DeleteLocalToTrash;
                        shape.subtree = true;  // folder deletion always takes the subtree
                        break;
                    case Action::None:
                        break;
                }
            }
            else
            {
                switch (action)
                {
                    case Action::LocalToRemote:
                        shape.type = OperationType::DeleteRemoteToRubbish;
                        shape.subtree = true;
                        break;
                    case Action::RemoteToLocal:
                    case Action::BestEffort:
                        shape.type = OperationType::Download;
                        shape.subtree = !hasExplicitDescendant;
                        break;
                    case Action::None:
                        break;
                }
            }
            return shape;
        }

        // Outermost single-sided directory ancestor whose shape is a subtree
        // operation. Rows under such a directory are covered by its subtree
        // op, so coverage points at the outermost one to keep enumeration
        // and aggregation consistent across nested uniform directories.
        QString outermostSubtreeDirAncestor(const QVector<Row>& rows,
                                            const QHash<QString, int>& rowByPath,
                                            const QHash<QString, DirOpShape>& dirShapes,
                                            const QString& path)
        {
            QString candidate;
            QString ancestor = parentPath(path);
            while (!ancestor.isEmpty())
            {
                const auto it = rowByPath.constFind(ancestor);
                if (it != rowByPath.constEnd() && isSingleSidedDirRow(rows.at(it.value())))
                {
                    const DirOpShape shape = dirShapes.value(ancestor);
                    if (shape.type != OperationType::None && shape.subtree)
                    {
                        candidate = ancestor;
                    }
                }
                ancestor = parentPath(ancestor);
            }
            return candidate;
        }

        bool hasExplicitDescendant(const QVector<Row>& rows, const QString& dirPath, const QHash<QString, Action>& decisions)
        {
            const QString prefix = dirPath + QLatin1Char('/');
            for (const Row& row : rows)
            {
                if (row.relativePath.startsWith(prefix) && decisions.contains(row.relativePath))
                {
                    return true;
                }
            }
            return false;
        }
    }

    const RowPlan* Plan::find(const QString& relativePath) const
    {
        for (const RowPlan& rowPlan : rows)
        {
            if (rowPlan.relativePath == relativePath)
            {
                return &rowPlan;
            }
        }
        return nullptr;
    }

    Plan Planner::plan(const Classification& classification, const QHash<QString, Action>& decisions) const
    {
        const QVector<Row>& classificationRows = classification.rows;
        Plan plan;
        plan.rows.resize(classificationRows.size());

        QHash<QString, int> rowByPath;
        for (int i = 0; i < classificationRows.size(); ++i)
        {
            rowByPath.insert(classificationRows.at(i).relativePath, i);
        }

        // Effective actions: own decision > nearest ancestor directory
        // decision > recommended action.
        QVector<Effective> effective(classificationRows.size());
        for (int i = 0; i < classificationRows.size(); ++i)
        {
            const Row& row = classificationRows.at(i);
            if (decisions.contains(row.relativePath))
            {
                effective[i].action = decisions.value(row.relativePath);
                effective[i].source = ActionSource::OwnDecision;
                continue;
            }

            QString ancestor = parentPath(row.relativePath);
            bool inherited = false;
            while (!ancestor.isEmpty())
            {
                if (decisions.contains(ancestor))
                {
                    effective[i].action = decisions.value(ancestor);
                    effective[i].source = ActionSource::InheritedFromDirectory;
                    effective[i].sourcePath = ancestor;
                    inherited = true;
                    break;
                }
                ancestor = parentPath(ancestor);
            }
            if (!inherited)
            {
                effective[i].action = row.recommendedAction;
                effective[i].source = ActionSource::Recommended;
            }
        }

        // Per-directory facts: explicit descendant decisions and the shape
        // of the operation the directory row itself emits.
        QHash<QString, bool> dirHasExplicitDescendant;
        QHash<QString, DirOpShape> dirShapes;
        for (int i = 0; i < classificationRows.size(); ++i)
        {
            const Row& row = classificationRows.at(i);
            if (!isSingleSidedDirRow(row))
            {
                continue;
            }
            const bool mixed = hasExplicitDescendant(classificationRows, row.relativePath, decisions);
            dirHasExplicitDescendant.insert(row.relativePath, mixed);
            dirShapes.insert(row.relativePath, dirOpShapeFor(row, effective.at(i).action, mixed));
        }

        // File rows.
        for (int i = 0; i < classificationRows.size(); ++i)
        {
            const Row& row = classificationRows.at(i);
            RowPlan& rowPlan = plan.rows[i];
            rowPlan.relativePath = row.relativePath;
            rowPlan.kind = row.kind;
            rowPlan.action = effective.at(i).action;
            rowPlan.actionSource = effective.at(i).source;
            rowPlan.decisionSourcePath = effective.at(i).sourcePath;

            if (row.kind == RowKind::Blocker)
            {
                const QVector<PlannedOperation> operations =
                    operationsForBlockerRow(row, effective.at(i).action, classification, rowPlan.warnings);
                if (operations.isEmpty())
                {
                    rowPlan.warnings.append(QStringLiteral("Blocked row: cannot be resolved by a plain transfer; the arrow actions transfer with an automatic rename"));
                }
                for (const PlannedOperation& operation : operations)
                {
                    rowPlan.operations.append(operation);
                    addConsequence(rowPlan, operation);
                }
                continue;
            }
            if (!isFileRow(row) || row.kind == RowKind::Identical)
            {
                continue;
            }

            // Coverage: a row without its own decision under a uniform
            // single-sided directory is covered by that directory's subtree
            // operation.
            if (effective.at(i).source != ActionSource::OwnDecision)
            {
                const QString dirPath = outermostSubtreeDirAncestor(classificationRows, rowByPath, dirShapes, row.relativePath);
                if (!dirPath.isEmpty())
                {
                    rowPlan.coveredByPath = dirPath;
                    continue;
                }
            }

            const QVector<PlannedOperation> operations =
                operationsForFileRow(row, effective.at(i).action, classification, rowPlan.warnings);
            for (const PlannedOperation& operation : operations)
            {
                rowPlan.operations.append(operation);
                addConsequence(rowPlan, operation);
                if (row.underBlockedPath)
                {
                    rowPlan.warnings.append(QStringLiteral("%1 sits under a path blocked by a type mismatch; the sync engine may stall this transfer")
                                                .arg(operation.path));
                }
            }
        }

        // Twin coupling (MEGA-2.9): an adopt rename on one row moves the
        // entry another row holds, so that row is covered by the adopt.
        // Rows are processed in classification order, so when both twins
        // carry explicit decisions the earlier (local-side) row's adopt
        // wins and the later row is superseded.
        for (int i = 0; i < classificationRows.size(); ++i)
        {
            for (const PlannedOperation& operation : plan.rows.at(i).operations)
            {
                if (!isRename(operation.type) || operation.fromPath == plan.rows.at(i).relativePath)
                {
                    continue;  // displacement renames act on the row's own entry
                }
                const auto twinIt = rowByPath.constFind(operation.fromPath);
                if (twinIt == rowByPath.constEnd() || twinIt.value() == i)
                {
                    continue;
                }

                RowPlan& twinPlan = plan.rows[twinIt.value()];
                if (twinPlan.actionSource == ActionSource::OwnDecision)
                {
                    twinPlan.warnings.append(QStringLiteral("Superseded by %1's rename: its content moves with the adopt")
                                                .arg(plan.rows.at(i).relativePath));
                }
                twinPlan.operations.clear();
                twinPlan.createdLocal.clear();
                twinPlan.changedLocal.clear();
                twinPlan.removedLocal.clear();
                twinPlan.createdRemote.clear();
                twinPlan.changedRemote.clear();
                twinPlan.removedRemote.clear();
                twinPlan.renamedLocal.clear();
                twinPlan.renamedRemote.clear();
                twinPlan.coveredByPath = plan.rows.at(i).relativePath;
            }
        }

        // Directory rows, deepest first so consequences aggregate upward:
        // a directory pulls in the already-computed consequences of nested
        // directories that plan their own operations.
        for (int i = classificationRows.size() - 1; i >= 0; --i)
        {
            const Row& row = classificationRows.at(i);
            if (!isDirRow(row))
            {
                continue;
            }
            RowPlan& rowPlan = plan.rows[i];
            rowPlan.relativePath = row.relativePath;
            rowPlan.kind = row.kind;
            rowPlan.action = effective.at(i).action;
            rowPlan.actionSource = effective.at(i).source;
            rowPlan.decisionSourcePath = effective.at(i).sourcePath;

            const QString dirPath = row.relativePath;
            const DirOpShape shape = dirShapes.value(dirPath);

            PlannedOperation ownOperation;
            if (shape.type != OperationType::None)
            {
                // Coverage of the directory row itself by an ancestor's
                // subtree operation.
                bool covered = false;
                if (effective.at(i).source != ActionSource::OwnDecision)
                {
                    const QString ancestorDir = outermostSubtreeDirAncestor(classificationRows, rowByPath, dirShapes, dirPath);
                    if (!ancestorDir.isEmpty())
                    {
                        rowPlan.coveredByPath = ancestorDir;
                        covered = true;
                    }
                }

                if (!covered)
                {
                    ownOperation.type = shape.type;
                    ownOperation.path = dirPath;
                    ownOperation.isFolder = true;
                    ownOperation.isSubtree = shape.subtree;
                    rowPlan.operations.append(ownOperation);
                }
            }

            // Consequences.
            const QString prefix = dirPath + QLatin1Char('/');
            for (int j = 0; j < classificationRows.size(); ++j)
            {
                if (j == i)
                {
                    continue;
                }
                const Row& other = classificationRows.at(j);
                if (!other.relativePath.startsWith(prefix))
                {
                    continue;
                }

                RowPlan& otherPlan = plan.rows[j];

                // An explicit deletion matching the subtree deletion is
                // redundant: the folder removal takes the entry with it.
                if (ownOperation.isSubtree && isDeletion(ownOperation.type) && otherPlan.actionSource == ActionSource::OwnDecision
                    && otherPlan.operations.size() == 1 && otherPlan.operations.first().type == ownOperation.type)
                {
                    otherPlan.operations.clear();
                    otherPlan.coveredByPath = dirPath;
                }

                if (ownOperation.isSubtree)
                {
                    switch (ownOperation.type)
                    {
                        case OperationType::Upload:
                            if (otherPlan.coveredByPath == dirPath && isFileRow(other))
                            {
                                rowPlan.createdRemote.append(other.relativePath);
                                if (!other.twinPath.isEmpty() && other.kind != RowKind::Identical)
                                {
                                    rowPlan.warnings.append(QStringLiteral("Uploading %1 duplicates identical content already on the remote side at %2")
                                                                .arg(other.relativePath, other.twinPath));
                                }
                            }
                            break;
                        case OperationType::Download:
                            if (otherPlan.coveredByPath == dirPath && isFileRow(other))
                            {
                                rowPlan.createdLocal.append(other.relativePath);
                                if (!other.twinPath.isEmpty() && other.kind != RowKind::Identical)
                                {
                                    rowPlan.warnings.append(QStringLiteral("Downloading %1 duplicates identical content already on the local side at %2")
                                                                .arg(other.relativePath, other.twinPath));
                                }
                            }
                            break;
                        case OperationType::DeleteLocalToTrash:
                            // A folder deletion takes every descendant with
                            // it, whatever the descendants decided.
                            rowPlan.removedLocal.append(other.relativePath);
                            break;
                        case OperationType::DeleteRemoteToRubbish:
                            rowPlan.removedRemote.append(other.relativePath);
                            break;
                        case OperationType::UploadReplace:
                        case OperationType::DownloadReplace:
                        case OperationType::RenameRemote:
                        case OperationType::RenameLocal:
                        case OperationType::None:
                            break;
                    }
                }

                if (otherPlan.coveredByPath != dirPath)
                {
                    // Descendant plans its own operations: aggregate its
                    // consequences and warnings into the directory row.
                    rowPlan.createdLocal += otherPlan.createdLocal;
                    rowPlan.changedLocal += otherPlan.changedLocal;
                    rowPlan.removedLocal += otherPlan.removedLocal;
                    rowPlan.createdRemote += otherPlan.createdRemote;
                    rowPlan.changedRemote += otherPlan.changedRemote;
                    rowPlan.removedRemote += otherPlan.removedRemote;
                    rowPlan.renamedLocal += otherPlan.renamedLocal;
                    rowPlan.renamedRemote += otherPlan.renamedRemote;
                    rowPlan.warnings += otherPlan.warnings;
                }

                // Keeper warning: an explicit decision keeps or creates
                // content the subtree deletion removes anyway.
                if (ownOperation.isSubtree && isDeletion(ownOperation.type) && otherPlan.coveredByPath != dirPath)
                {
                    for (const PlannedOperation& operation : otherPlan.operations)
                    {
                        if (!isDeletion(operation.type))
                        {
                            rowPlan.warnings.append(QStringLiteral("Deleting %1 also removes %2 despite its explicit decision")
                                                        .arg(dirPath, other.relativePath));
                            break;
                        }
                    }
                }
            }

            if (ownOperation.type != OperationType::None)
            {
                // The folder itself is created/placed (subtree transfers),
                // or goes away with the subtree (deletions), or is
                // created/placed alone (node-only ops).
                addConsequence(rowPlan, ownOperation);
            }
        }

        return plan;
    }
}