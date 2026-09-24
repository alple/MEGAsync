#include "SyncPreviewPlanner.h"

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

        void addConsequence(RowPlan& plan, OperationType type, const QString& path)
        {
            switch (type)
            {
                case OperationType::Upload:
                    plan.createdRemote.append(path);
                    break;
                case OperationType::Download:
                    plan.createdLocal.append(path);
                    break;
                case OperationType::UploadReplace:
                    plan.changedRemote.append(path);
                    break;
                case OperationType::DownloadReplace:
                    plan.changedLocal.append(path);
                    break;
                case OperationType::DeleteRemoteToRubbish:
                    plan.removedRemote.append(path);
                    break;
                case OperationType::DeleteLocalToTrash:
                    plan.removedLocal.append(path);
                    break;
                case OperationType::None:
                    break;
            }
        }

        // Operation for one file row from its effective action. Duplicate and
        // best-effort advisory warnings are appended to `warnings`.
        PlannedOperation operationForFileRow(const Row& row, Action action, QStringList& warnings)
        {
            PlannedOperation operation;
            operation.path = row.relativePath;

            if (action == Action::None)
            {
                return operation;
            }

            const bool hasLocal = row.local.has_value();
            const bool hasRemote = row.remote.has_value();

            if (hasLocal && !hasRemote)
            {
                switch (action)
                {
                    case Action::LocalToRemote:
                    case Action::BestEffort:
                        operation.type = OperationType::Upload;
                        break;
                    case Action::RemoteToLocal:
                        operation.type = OperationType::DeleteLocalToTrash;
                        break;
                    case Action::None:
                        break;
                }
                if (operation.type == OperationType::Upload && row.kind == RowKind::Conflict && !row.twinPath.isEmpty())
                {
                    warnings.append(QStringLiteral("Uploading %1 duplicates identical content already on the remote side at %2")
                                        .arg(row.relativePath, row.twinPath));
                }
            }
            else if (!hasLocal && hasRemote)
            {
                switch (action)
                {
                    case Action::LocalToRemote:
                        operation.type = OperationType::DeleteRemoteToRubbish;
                        break;
                    case Action::RemoteToLocal:
                    case Action::BestEffort:
                        operation.type = OperationType::Download;
                        break;
                    case Action::None:
                        break;
                }
                if (operation.type == OperationType::Download && row.kind == RowKind::Conflict && !row.twinPath.isEmpty())
                {
                    warnings.append(QStringLiteral("Downloading %1 duplicates identical content already on the local side at %2")
                                        .arg(row.relativePath, row.twinPath));
                }
            }
            else
            {
                // Both sides present, differing.
                switch (action)
                {
                    case Action::LocalToRemote:
                        operation.type = OperationType::UploadReplace;
                        break;
                    case Action::RemoteToLocal:
                        operation.type = OperationType::DownloadReplace;
                        break;
                    case Action::BestEffort:
                        warnings.append(QStringLiteral("Best-effort cannot merge differing content of %1; flagged as conflict, no transfer planned")
                                            .arg(row.relativePath));
                        break;
                    case Action::None:
                        break;
                }
            }

            return operation;
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
                rowPlan.warnings.append(QStringLiteral("Blocked row: cannot be resolved by a transfer; resolve by rename or exclusion"));
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

            const PlannedOperation operation = operationForFileRow(row, effective.at(i).action, rowPlan.warnings);
            if (operation.type != OperationType::None)
            {
                rowPlan.operations.append(operation);
                addConsequence(rowPlan, operation.type, operation.path);
                if (row.underBlockedPath)
                {
                    rowPlan.warnings.append(QStringLiteral("%1 sits under a path blocked by a type mismatch; the sync engine may stall this transfer")
                                                .arg(operation.path));
                }
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
                                if (other.kind == RowKind::Conflict && !other.twinPath.isEmpty())
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
                                if (other.kind == RowKind::Conflict && !other.twinPath.isEmpty())
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
                addConsequence(rowPlan, ownOperation.type, ownOperation.path);
            }
        }

        return plan;
    }
}
