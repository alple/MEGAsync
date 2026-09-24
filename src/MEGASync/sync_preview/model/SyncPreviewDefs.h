#ifndef SYNCPREVIEWDEFS_H
#define SYNCPREVIEWDEFS_H

namespace SyncPreview
{
    enum class EntryType
    {
        File,
        Folder
    };

    // The six row kinds the classifier produces (MEGA-2.1 acceptance
    // criteria #3). Conflict rows are same-content-different-name pairs kept
    // as separate per-side rows (one row per side's entry), flagged with the
    // advisory twin flag; blocker rows cannot be resolved by a transfer.
    enum class RowKind
    {
        LocalOnly,
        RemoteOnly,
        Identical,
        BothDiffer,
        Conflict,
        Blocker
    };

    enum class BlockerReason
    {
        None,
        TypeMismatch,
        CaseInsensitiveNameCollision
    };

    // The three user actions per row (file or directory) plus an explicit
    // none. L->R makes remote like local; R->L makes local like remote;
    // best-effort is a both-way merge where same-name-differ surfaces as a
    // conflict.
    enum class Action
    {
        LocalToRemote,
        RemoteToLocal,
        BestEffort,
        None
    };

    // Operation vocabulary: only actions verified to exist in the real API
    // (MEGA-2 decision records): upload, download, moveNodeToRubbish (remote
    // deletions), local trash/backup deletion, none. The two replace types
    // are single logical operations the enforcement engine expands into the
    // verified primitive sequences (upload + moveNodeToRubbish(old);
    // download with local collision handling). Nothing ever hard-unlinks:
    // remote items go to MEGA Rubbish, local items go to the OS trash or
    // backup folder.
    enum class OperationType
    {
        Upload,
        Download,
        UploadReplace,
        DownloadReplace,
        DeleteRemoteToRubbish,
        DeleteLocalToTrash,
        None
    };
}

#endif // SYNCPREVIEWDEFS_H
