#ifndef SYNCPREVIEWPLANNER_H
#define SYNCPREVIEWPLANNER_H

#include "SyncPreviewClassifier.h"
#include "SyncPreviewDefs.h"

#include <QHash>
#include <QPair>
#include <QStringList>

namespace SyncPreview
{
    enum class ActionSource
    {
        Recommended,             // no explicit decision anywhere above the row
        InheritedFromDirectory,  // cascaded from a directory decision
        OwnDecision              // explicit decision on this row
    };

    // One planned operation. Folder-scoped ops carry the folder path; the
    // enforcement engine (Stage 4) expands subtree ops and the replace ops
    // into the verified primitive sequences. Rename ops (MEGA-2.9) carry
    // fromPath -> toPath; `path` stays the row path the decision was made
    // on.
    struct PlannedOperation
    {
        OperationType type = OperationType::None;
        QString path;
        bool isFolder = false;
        bool isSubtree = false;  // folder op covering the whole subtree
        QString fromPath;        // rename ops: moved path
        QString toPath;          // rename ops: destination path
    };

    // Consequences for one row. File rows list their own effects (or
    // nothing when a directory subtree op covers them); directory rows
    // aggregate their whole cascaded subtree for the consequences popup.
    struct RowPlan
    {
        QString relativePath;
        RowKind kind = RowKind::LocalOnly;
        Action action = Action::None;  // effective action applied
        ActionSource actionSource = ActionSource::Recommended;
        QString decisionSourcePath;    // set when actionSource == InheritedFromDirectory
        QString coveredByPath;         // set when a directory subtree op covers this row

        QVector<PlannedOperation> operations;

        QStringList createdLocal;
        QStringList changedLocal;  // content overwritten (old copy recoverable)
        QStringList removedLocal;  // moved to OS trash/backup
        QStringList createdRemote;
        QStringList changedRemote;  // content overwritten (old copy to Rubbish)
        QStringList removedRemote;  // moved to MEGA Rubbish
        // Rename consequences (MEGA-2.9): moved paths as (from, to) pairs.
        // Renames transfer no bytes and remove nothing, so the pending
        // summary ignores them.
        QVector<QPair<QString, QString>> renamedLocal;
        QVector<QPair<QString, QString>> renamedRemote;
        QStringList warnings;       // advisory notes: duplicates, blocked paths, structural overrides
    };

    struct Plan
    {
        QVector<RowPlan> rows;  // mirrors the classification row order

        const RowPlan* find(const QString& relativePath) const;
    };

    // Turns chosen actions into an operation list with consequences.
    // Decisions: relativePath -> explicit Action for rows the user set
    // (files or directories). Effective decision of a row: its own explicit
    // decision, else the nearest ancestor directory's explicit decision,
    // else the row's recommended action. Pure function of the
    // classification; the vocabulary is limited to the verified operation
    // types (see SyncPreviewDefs.h).
    class Planner
    {
    public:
        Plan plan(const Classification& classification, const QHash<QString, Action>& decisions) const;
    };
}

#endif // SYNCPREVIEWPLANNER_H
