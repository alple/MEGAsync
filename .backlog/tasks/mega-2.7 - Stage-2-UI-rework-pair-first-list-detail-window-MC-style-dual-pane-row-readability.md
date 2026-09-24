---
id: MEGA-2.7
title: >-
  Stage 2 UI rework: pair-first list + detail window, MC-style dual pane, row
  readability
status: To Do
assignee: []
created_date: '2026-09-24 14:45'
labels:
  - sync-preview
milestone: Sync pre-commit review
dependencies:
  - MEGA-2.2
parent_task_id: MEGA-2
priority: high
type: enhancement
ordinal: 30000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up from MEGA-2.2 Testing feedback (2026-09-24): the delivered review dialog works functionally but the user rejected its information architecture and readability.

Context (what exists today, MEGA-2.2):
- SyncPreviewDialog shows ALL pairs and rows in a single QTreeWidget: pair header nodes with row children, action/approval in the right-most column (col 6), dual-pane data as plain tree columns (path | local size | local modified | remote size | remote modified | newer side | action & approval).
- Files: src/MEGASync/sync_preview/gui/ — SyncPreviewDialog.{h,cpp} + gui/ui/SyncPreviewDialog.ui (7-column QTreeWidget), SyncPreviewRowWidget.{h,cpp} (the col-6 widget: action combo + approve checkbox), SyncPreviewPairController.{h,cpp} (headless queue/decisions/gate/persistence), SyncPreviewConsequencesDialog, SyncPreviewFakePairPicker. Persistence: <data dir>/sync-preview-queue.json via SyncPreviewQueueFileStore; fake scenarios keyed by pair label (SyncPreviewFakePairPicker).

User's requested modifications (verbatim intent):
1. "It was supposed to be double pane, like midnight commander with actions in the middle" — local pane left, remote pane right, the action choice sits BETWEEN the panes (per row), not in a right-hand column.
2. "It's confusing to have every pair in one table with each of the details" — replace the combined table with a pair-first flow: a list of pairs with per-pair summary details (how big is the whole sync: size / number of files / number of dirs "if it's easily accessible"), and opening a pair opens ANOTHER WINDOW with the details.
3. Keep the "Show in-sync items" checkbox (user liked it).
4. "The color palette is hard to read for rows that are greyed out or not selected."

Implementation notes:
- Pair summary is cheaply accessible from the classification the controller already holds: per side, sum file sizes / count files / count dirs over the classification rows (dedupe identical rows for the pair-level view). Suggested additive controller API: PairSummary summary(pairId) → e.g. {localBytes, localFiles, localDirs, remoteBytes, remoteFiles, remoteDirs} (+ optionally the pending-change delta created/changed/removed per side from the plan). OPEN QUESTION for the user (ask when starting): should "size of the whole sync" mean the full subtree size or the delta that would actually transfer? Cheap to expose both.
- Detail window: a second QDialog opened per pair (opens from the pair list; non-modal like the main dialog, or modal — pick what feels right and note it). MC-style: 3-column row layout (local info | action combo + approve | remote info) — implement as per-row setItemWidget in a tree with the middle column holding actions, or a custom row widget spanning; keep the tree/row-widget machinery from Stage 2 where possible, restructure rather than rewrite.
- Palette: use the prod theme system (TokenParserWidgetManager / theme tokens — check how StalledIssuesDialog and other dialogs pick colors) instead of hardcoded grey; specifically fix rows shown greyed/disabled (blocker combos disabled rows read poorly) and unselected rows. If both themes can't be covered cheaply, at least match app conventions and note the limitation.
- Pair list view: show pair label + summary stats + Remove + gated Commit (or commit per detail window — decide during work; keep the gate on all flagged rows approved).
- The pair label doubles as the fake-scenario key (FakePairPicker); keep that mechanism.
- Stage-5 touch points unchanged (MegaApplication hook + menu item already exist); wizard button and real commit flow still Stage 5.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Pair list view: the dialog's initial view lists queued pairs with per-pair summary details (per-side total size, file count, dir count) instead of one combined table; opening a pair opens a separate detail window
- [ ] #2 Detail window is a true dual-pane view, Midnight Commander style: local side left, remote side right, action selection in the middle between the panes (L→R / R→L / best-effort), per-side size/modified-date columns kept
- [ ] #3 The 'Show in-sync items' checkbox exists in the detail window and works as today (identical rows hidden by default)
- [ ] #4 Greyed-out/disabled and unselected rows are readable: colors come from the app's theme/palette conventions rather than ad-hoc colors, with sufficient contrast in the active theme (both light and dark if cheaply possible)
- [ ] #5 Filtering split: pair-name filter on the pair list view, path filter inside the detail window; the former 'group by pair' toggle is removed as redundant with the pair-first design
- [ ] #6 Scope guard: still fake data only (Stage 2); commit gating, directory-action consequences popup, load-more cap and multi-pair add/remove preserved; Stage-1 core untouched except an additive summary computation; no real API calls
- [ ] #7 just build compiles clean and just test stays green
<!-- AC:END -->
