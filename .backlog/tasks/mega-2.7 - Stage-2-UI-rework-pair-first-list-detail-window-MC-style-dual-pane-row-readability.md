---
id: MEGA-2.7
title: >-
  Stage 2 UI rework: pair-first list + detail window, MC-style dual pane, row
  readability
status: Testing
assignee:
  - agent
created_date: '2026-09-24 14:45'
updated_date: '2026-09-24 18:49'
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
- [x] #1 Pair list view: the dialog's initial view lists queued pairs with per-pair summary details (per-side total size, file count, dir count) instead of one combined table; opening a pair opens a separate detail window
- [x] #2 Detail window is a true dual-pane view, Midnight Commander style: local side left, remote side right, action selection in the middle between the panes (L→R / R→L / best-effort), per-side size/modified-date columns kept
- [x] #3 The 'Show in-sync items' checkbox exists in the detail window and works as today (identical rows hidden by default)
- [x] #4 Greyed-out/disabled and unselected rows are readable: colors come from the app's theme/palette conventions rather than ad-hoc colors, with sufficient contrast in the active theme (both light and dark if cheaply possible)
- [x] #5 Filtering split: pair-name filter on the pair list view, path filter inside the detail window; the former 'group by pair' toggle is removed as redundant with the pair-first design
- [x] #6 Scope guard: still fake data only (Stage 2); commit gating, directory-action consequences popup, load-more cap and multi-pair add/remove preserved; Stage-1 core untouched except an additive summary computation; no real API calls
- [x] #7 just build compiles clean and just test stays green
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Stage 2 UI rework plan (MEGA-2.7)

1. Stage-1 core — ADDITIVE ONLY
   - SyncPreviewPairController.{h,cpp}: add PairSummary {localBytes, localFiles, localDirs, remoteBytes, remoteFiles, remoteDirs} + delta fields (per side: pending created/changed/removed paths + their bytes) and PairController::summary(pairId) computed from the classification + current plan. Pure additive; no behavior change.
   - Unit tests in SyncPreviewPairControllerTests.cpp for summary() (subtree stats + delta).

2. Main dialog → pair list view (SyncPreviewDialog.ui rework)
   - Replace the 7-column rowsTree with a pair list (QListWidget + per-row item widget): pair label (bold, doubles as fake-scenario key — unchanged), summary line (per-side size / files / dirs + pending delta + awaiting-approval count), gated Commit button (tooltip: Stage 5) + Remove button (confirm, as today).
   - Filter repurposed to pair-name filter (list-level only). groupByPairToggle removed (AC #5). showInSyncToggle moves to the detail window (AC #3).
   - Footer summary + Close + Add pair… preserved. Controller signals refresh the list.

3. Detail window (new SyncPreviewPairDetailDialog.{h,cpp,ui})
   - Opened per pair from the list; one window per pair (re-open raises the existing). NON-MODAL, own Qt::Window, WA_DeleteOnClose (noted decision: matches the non-modal main dialog, allows several pairs open for comparison).
   - MC-style dual pane: tree columns [Local path | Local size | Local modified | Action & approval | Remote path | Remote size | Remote modified] — action combo + approve checkbox sit in the MIDDLE between the panes via setItemWidget; SyncPreviewRowWidget reused as-is.
   - Path filter + Show in-sync items checkbox live here; kRowCap load-more preserved per pair.
   - Row interactions move here from SyncPreviewDialog: directory-action consequences popup (SyncPreviewConsequencesDialog flow), setAction/setApproved via the shared PairController*.
   - Listens to pairRemoved → self-close if its pair vanishes.

4. Readability (AC #4) — theme conventions
   - All colors from TokenParserWidgetManager tokens (getColor / /*colorToken.*/ stylesheet markers): text-primary for rows, text-secondary for de-emphasized rows (identical/covered), text-error for CONFLICT/BLOCKER badges, text-warning for re-approve; alternating row background from surface-1/page-background; disabled-group foreground on row widgets raised from Qt default to readable token colors.
   - Both dialogs registered with TokenParserWidgetManager (registerWidgetForTheming) so prod standard styling applies and live theme changes re-theme.
   - Re-populate on theme change so per-row token colors update (cheap hook on ThemeManager::themeChanged).

5. Wiring
   - syncpreview.cmake: add new detail-window files + .ui.
   - Stage-5 touch points unchanged; fake data only (AC #6); just build + just test green (AC #7).

Risks/notes:
- Builds on MEGA-2.2/MEGA-2.1 which are in Testing (unreviewed) — soft-frontier start.
- OPEN QUESTION for user: pair summary size = full subtree vs pending delta vs both (task flags this; cheap to expose both).
- Commit placement decision: keep Commit + Remove on the pair list rows (current grouped-mode behavior moved over); detail window is review-only.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Claimed by agent session 2026-09-24. Blockers MEGA-2.2/MEGA-2.1 in Testing — startable under soft-frontier rule; builds on unreviewed work.

Feedback 2026-09-24 (Testing): the delivered detail view is one 7-column table, not two visible panes — user wants separate left/right TREE panes, synchronized (MC-style). Reworking the detail window into three side-by-side views: local tree | action strip | remote tree, row-locked.

2026-09-24: second Testing feedback — "ugly and not usable, could we integrate some tree comparator like meld". Direction decided with user: port meld's folder-diff presentation in-app; embedding/external meld ruled out (GPLv2 + GTK; remote side is cloud data). Split off into MEGA-2.8 (meld-style per-side states, status columns, Same/Different/New filters, entry icons). This ticket stays in Testing for the pair-first list + two-pane lock-step mechanics; look-and-feel rework is MEGA-2.8.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Stage 2 UI rework (MEGA-2.7) — second iteration per Testing feedback: "it doesn't have the two pane! I should be able to see the tree on left and right panes synchronized".

## The fix

The first delivery put all seven columns in ONE QTreeWidget — visually a single table, not two panes. The detail window is now three side-by-side views inside a QSplitter (user-resizable side panes, fixed action strip):

- **Left pane** (`leftTree`): local tree — columns Path | Size | Modified
- **Middle strip** (`midTree`, fixed 380px): one column "Action & approval", the existing `SyncPreviewRowWidget` (combo L→R / R→L / Best-effort / None + approve checkbox) per row via setItemWidget
- **Right pane** (`rightTree`): remote tree — columns Path | Size | Modified

### Synchronization (row lock-step)
- Every classified path is ONE row index on all three views: each visible row is added to left/mid/right in lock-step. A side that misses the entry shows "—" in its pane cell, so panes always line up.
- Uniform fixed row height (40px) on every item in every view — identical row geometry is what makes the lock-step hold.
- Vertical scroll sync three-way (left ↔ mid ↔ right) with a reentrancy guard.
- Selection sync: selecting a row in the left pane selects the same row in the right pane and vice versa (middle strip is non-selecting).
- Alternating banding (page-background / surface-1 tokens) on both tree panes reads as one continuous band across panes; splitter is non-collapsible; middle strip has fixed min/max width.
- Each pane spells the entry as its own side holds it (conflict rows may differ per side); badges/tooltips sit on the left pane's path cell.

### Kept from the first delivery
Pair list main dialog unchanged; per-pair path filter + Show-in-sync toggle + kRowCap load-more; consequences popup flow; re-flag badges/tooltips; theme-token colors and palettes (now applied to all three views); rebuild on ThemeManager::themeChanged; non-modal one-window-per-pair detail windows; self-close on pair removal.

## Verification
- `just build` compiles clean; `just test` all green (108 test cases, 646 assertions). Model untouched in this pass (no new controller changes).
- Visual QA pending user testing: panes should scroll/select in lock-step; drag the splitter to resize the two panes.

## Testing notes (unchanged)
- "Pending" is non-empty with no explicit decisions — Stage-1 planner cascades single-sided directories by recommendation.
- Delta bytes = source-side sizes of created+changed files; removals counted, not summed.
<!-- SECTION:FINAL_SUMMARY:END -->
