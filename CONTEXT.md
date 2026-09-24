# MEGAsync (fork)

The MEGAsync desktop app — production sync client — extended by this private fork with additive, review-oriented features. The upstream sync engine remains the authority for all sync decisions; fork additions observe, report, and only act through existing app APIs.

## Language

**Sync preview**:
The advisory review of what creating a sync between a local and a remote folder would transfer, shown before the sync exists. Advisory only: the sync engine stays the authority on everything the preview reports.
_Avoid_: dry run (no such API exists), pre-sync scan

**Pair**:
A local folder and a remote folder the user considers syncing together. The unit the review dialog queues, persists, and commits.

**Row**:
One reviewable line in the preview: a file or folder as seen from one or both sides of a pair. Every row carries one action and one approval state.
_Avoid_: item, entry (entry = one side's file or folder record)

**Local-only**:
A row whose file or folder exists on the local side of the pair only.

**Remote-only**:
A row whose file or folder exists on the remote side only.

**Identical**:
A row whose content exists on both sides and is equal (size short-circuit, then CRC). Hidden by default in the UI.

**Both-differ**:
A row whose file exists under the same name on both sides with different content. Always flagged for explicit approval.
_Avoid_: conflict (reserved — see below)

**Conflict**:
The same content living under different names on opposite sides. Kept as two separate, independently decidable rows, each carrying an advisory flag pointing at its counterpart.
_Avoid_: both-differ (a different row kind), duplicate

**Twin**:
The identical-content counterpart of a conflict row on the other side, under a different name.

**Blocker**:
A row no transfer can resolve — a file-vs-folder type mismatch, or a case-insensitive name collision. Requires rename or exclusion (out of the preview's scope); blocks commit until acknowledged.
_Avoid_: conflict (blockers are their own kind)

**Action**:
The user's per-row choice: **L→R** (make remote like local), **R→L** (make local like remote), **best-effort** (both-way merge; same-name-differ surfaces as needing a decision). An explicit **none** is also an action.
_Avoid_: resolution (conflates action with approval)

**Recommended action**:
The pre-marked default per row, always overridable: single-sided rows get the bring-over action, identical rows get none, both-differ rows get the newer side (ties: L→R), conflicts and blockers get none.

**Decision**:
A user-chosen action recorded on a row. Explicit decisions cascade to contained rows; a row's effective decision is its own, else the nearest ancestor directory's, else the recommended action.
_Avoid_: approval (approval is the separate commit gate)

**Operation**:
A planned primitive using only verified real-API actions: upload, download, upload-replace, download-replace (each replace a single logical op the enforcement expands into upload/download + recoverable old-copy removal), remote deletion (→ MEGA Rubbish), local deletion (→ OS trash/backup). Nothing hard-unlinks.

**Plan**:
The operation list plus consequences computed from the decisions over a classification. Pure function of the classification; no UI, no I/O.

**Consequences**:
What changes where if a plan (or one row's action) runs: per side, which paths are created, changed (old copy recoverable), or removed (recoverable). Shown before approving a directory-level action.

**Queue**:
The persisted list of queued pairs with their per-row decisions and approvals, surviving window closes and app restarts in a fork-owned JSON file. Pairs drop off when their sync is created; trees are never persisted — the queue is re-scanned on reopen.

**Provider seam**:
The interface boundary (local-side, remote-side) between the preview core and its data sources, letting the review dialog run on generated fake trees before, and unchanged with, real providers.

**Fake provider**:
The seam implementation generating trees with edge cases (nested dirs, empty dirs, same-name-diff-content, same-content-diff-name, type mismatches, case collisions, deep trees) for GUI development and demoing before real integration.
