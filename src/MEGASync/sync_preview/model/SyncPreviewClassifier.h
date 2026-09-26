#ifndef SYNCPREVIEWCLASSIFIER_H
#define SYNCPREVIEWCLASSIFIER_H

#include "SyncPreviewProviders.h"

#include <optional>

namespace SyncPreview
{
    // One review row: a file or folder as seen from both sides. Conflict
    // rows are same-content-different-name pairs kept as separate per-side
    // rows (each row holds exactly one side's entry) with an advisory twin
    // flag; blocker rows hold both entries of a type mismatch or a
    // case-insensitive name collision in a single row.
    struct Row
    {
        // Canonical path: the local spelling when both sides exist, else
        // the existing side's spelling.
        QString relativePath;
        RowKind kind = RowKind::LocalOnly;
        std::optional<Entry> local;
        std::optional<Entry> remote;

        // Conflict and blocker rows require explicit approval before commit.
        bool requiresApproval = false;
        // Advisory: an identical-content counterpart exists elsewhere under
        // a different name. Conflict rows carry their one twin (the other
        // row's path); paired rows can carry up to two, one per transfer
        // direction (MEGA-2.9): `twinPath` is a counterpart of the row's
        // LOCAL content on the remote side (the L->R adopt target), and
        // `localTwinPath` is a counterpart of the row's REMOTE content on
        // the local side (the R->L adopt target). Single-sided rows hold
        // only `twinPath` (their counterpart lives on the other side). The
        // flags never change the row kind.
        bool hasIdenticalTwin = false;
        QString twinPath;
        QString localTwinPath;
        // Advisory: the entry lives under a path blocked by a file-vs-folder
        // type mismatch.
        bool underBlockedPath = false;
        // Valid when kind == Blocker.
        BlockerReason blockerReason = BlockerReason::None;

        Action recommendedAction = Action::None;
    };

    struct Classification
    {
        // Stable order: local tree order (paired, merged and local-only
        // rows interleaved), then remote-only leftovers in remote tree
        // order.
        QVector<Row> rows;

        const Row* find(const QString& relativePath) const;
    };

    // Pairs two side snapshots into classification rows. Content equality is
    // size short-circuit, then CRC (see sameContent). Pure function of the
    // provider snapshots: no filesystem, no SDK.
    class Classifier
    {
    public:
        Classification classify(const LocalSideProvider& localSide, const RemoteSideProvider& remoteSide) const;
    };
}

#endif // SYNCPREVIEWCLASSIFIER_H
