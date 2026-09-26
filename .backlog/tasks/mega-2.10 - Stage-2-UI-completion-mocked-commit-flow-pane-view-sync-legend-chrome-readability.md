---
id: MEGA-2.10
title: >-
  Stage 2 UI completion: mocked commit flow, pane view sync + legend, chrome
  readability
status: Testing
assignee:
  - '@agent'
created_date: '2026-09-26 15:25'
updated_date: '2026-09-26 15:41'
labels:
  - sync-preview
milestone: Sync pre-commit review
dependencies: []
parent_task_id: MEGA-2
priority: high
type: enhancement
ordinal: 33000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Spun out of MEGA-2.9 Testing feedback (2026-09-26). The review UI must be fully functional on fake data BEFORE any production API work (staging gate, re-staged epic MEGA-2 stage 3). All items below are fork-side sync_preview only, fake data only, nothing upstream-touched.

**1. Mocked commit flow (replaces the "Stage 5" placeholder):**
- The pair list's Commit pair button currently disables until allApproved and pops "The commit flow arrives in Stage 5" — the tester could not test commit at all. No "later stage" text may remain anywhere in the review UI.
- New flow (mocked): commit gated on allApproved (buttons stay enabled-state driven) → confirmation popup → applies the plan to the fake data via the installed plan applier (same seam the Stage-5 real flow plugs into) → re-verifies → marks the pair completed → the pair leaves the queue (list row gone, detail window closes, persisted). Re-adding the same scenario shows the post-commit state (everything identical) — the "sync happened" demonstration.
- Controller seam: `commitPair(pairId)` — applier optional (tests), apply-then-drop.

**2. Pane view synchronization (left/right same shape):**
- Root cause found: `makeSideItem` applies expansion flags only to folders existing on that side, so remote-only dirs render expanded on the right but collapsed (struck) on the left — the reviewer sees "less on the left, more on the right".
- Expansion flags must apply to folder ROWS on both panes regardless of side presence (missing sides stay struck-through, but expand/collapse in lock-step).
- A "Synchronize view" button (top chrome) that normalizes both panes to the same expansion state (mirror the left pane's per-path expansion onto the right, including selection/scroll lock-step re-application).

**3. Filter-row chrome readability (reported black-on-dark):**
- The Show changes/Apply buttons next to the filter render with unreadable text on the dark schema (the reviewer could not even discover Show changes). Style them with the proven readable quiet sheet the action panel uses (token-resolved, re-applied on theme change); Close button included for consistency. Verify both color schemas.

**4. Legend (top-right of the detail window):**
- State colors are currently undiscoverable: blue = Modified (differs between the sides), green = New (present on one side only), red = Blocked (cannot sync as-is), gray = Same, gray struck-through = Missing on that side. Add a compact legend at the top-right of the pair review window (chip-styled, token-colored, re-resolving on theme change).

**5. Functional-UI completeness sweep (staging-gate closure):**
- Walk every button in both windows on fake data: decisions, approvals, consequences popup, show changes, apply loop, commit mock, filters, load-more, add/remove pair, synchronize view — all functional, operations registered and visible. Anything found dead gets fixed or ticketed here.

Acceptance Criteria:
- No "later stage"/"Stage 5" text remains in the review UI; commit works end-to-end on fake data.
- Both panes show the same expansion shape for the same path at the same depth; "Synchronize view" normalizes them.
- Filter-row buttons readable in both color schemas.
- Legend present top-right, matches the actual state colors.
- just build + just test green; scope stays sync_preview-only on fake data.
<!-- SECTION:DESCRIPTION:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
## Implementation plan (recorded before implementation, 2026-09-26)

Builds on the uncommitted MEGA-2.9 diff (Testing). All changes inside `sync_preview/` + tests; nothing upstream-touched; fake data only.

### 1. Mocked commit flow
- `PairController::commitPair(pairId)`: gated on `allApproved` → runs the installed plan applier (optional, so tests work without one) → `removePair` (persists + emits `pairRemoved`; existing detail-window close-on-remove wiring handles the UI). Same seam the Stage-5 real commit replaces.
- Pair list: Commit pair click → confirmation popup (states what happens: plan applied to the pair's fake data, pair leaves the queue; labeled mocked) → `commitPair`. Tooltip updated; no "Stage 5"/"later stage" text anywhere. Buttons already enabled-state driven by `allApproved` (rebuilt on every pairChanged).

### 2. Pane view synchronization
- Root-cause fix in `makeSideItem`: apply the expansion flag to every folder ROW (the row-level `isFolder` already computed for icons), not only folders present on that side — missing sides stay struck-through but expand/collapse in lock-step. This removes the left-collapsed/right-expanded asymmetry.
- New "Synchronize view" outline button in the filter row: mirrors the LEFT pane's per-path expansion onto the right pane (records into `mExpandedByPath`, guarded by `mSyncingPanes`), then re-applies selection/scroll lock-step.

### 3. Filter-row chrome readability
- `applyPanesPalette` applies the action-panel's proven token-resolved quiet sheet (`GuiStyle::actionButtonStyleSheet()`) to Close, Show changes, Apply and Synchronize view — re-resolved on theme change (existing connect). Property-based outline styling stays (harmless under the per-widget sheet).

### 4. Legend (top-right)
- Detail window header becomes a row: pair label (stretch) + legend rich-text label: colored dot + word per state — Modified (text-info), New (text-success), Blocked (text-error), Same (text-secondary), Missing (text-secondary, struck). Built in `applyPanesPalette` so tokens re-resolve on theme change; reuses the translated `stateText` words.

### 5. Completeness sweep
- Verify every button in both windows drives something on fake data (decisions, approve, consequences, show changes, apply, commit, filters, load-more, add/remove pair, synchronize view); fix or note any dead control.

### Tests
- Controller: `commitPair` — refuses unapproved, applies + drops (applier installed), drops without applier; queue persisted empty; detail-close behavior is GUI-side (no widget harness, consistent with existing practice).
- Existing suites stay green (removal of the Stage-5 popup doesn't touch controller tests).

### Order
Controller commitPair → pair-list commit flow → detail expansion fix + Synchronize view → chrome sheet → legend → build + test → finish (Testing + summary).
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Implemented MEGA-2.10 on top of the uncommitted MEGA-2.9 diff. `just build` clean, `just test` green: **922 assertions / 135 cases**. Diff confined to `sync_preview/` + its tests + the regenerated translation source; fake data only, nothing upstream-touched.

**1. Mocked commit flow (the "Stage 5" placeholder is gone):**
- `PairController::commitPair(pairId)`: gated on `allApproved` → applies the plan through the installed plan applier (fake data) → drops the pair from the queue (`removePair`: persists + `pairRemoved`, so the list row vanishes and open detail windows close). The applier is optional — without one the drop still stands in for sync creation. This is the same seam the Stage-5 real commit flow replaces.
- Pair list: Commit pair → confirmation popup ("Apply the approved plan to this pair's fake data and drop it from the review queue (mocked sync creation)?") → commit. Tooltip updated. Re-adding the same scenario re-scans to the post-commit state — the pinned test shows the whole pair reading Identical ("the sync happened").
- No "later stage"/"Stage 5" text remains anywhere in the review UI (grep-verified).

**2. Pane view synchronization:**
- Root cause fixed in `makeSideItem`: the expansion flag now applies to every folder ROW on both panes (the row-level `isFolder` computed for icons), not only folders present on that side — remote-only dirs stay struck-through on the left but expand/collapse in lock-step. Both panes open with the same shape at the same depth.
- New **Synchronize view** button in the filter row: mirrors the left pane's per-path expansion onto the right pane (records into the expansion map, signal-guarded).

**3. Filter-row chrome readability (the reported black-on-dark):**
- `applyPanesPalette` now applies the action panel's token-resolved quiet sheet (`GuiStyle::actionButtonStyleSheet()`) to Close, Show changes, Apply and Synchronize view — re-resolved on theme change. The property-outline styling evidently failed to tokenize in this window; the per-widget sheet sidesteps it and is the same proven-readable treatment the action buttons use.

**4. Legend (top-right):**
- The detail window header is now a row: pair label (stretch) + legend — colored dots over the exact pane tokens: blue = Modified, green = New, red = Blocked, gray = Same, gray struck = Missing. Rebuilt in `applyPanesPalette`, so it re-resolves on theme change.

**ADJUSTMENT BEYOND THE RECORDED PLAN (flagging for review):** `setApproved` no longer *creates* decisions — approval now attaches to an existing decision only. The commit test exposed that approve-only decisions (action = recommended, e.g. None on conflicts) made the planner read them as explicit choices: they turned directory cascades node-only and emptied the commit plan, so an approve-only commit silently transferred nothing. The UI could never produce this state (the Approve toggle is hidden for effective=None rows since MEGA-2.8) — only the API could. Flow is now: decide first, then approve (the UI already enforces this). Two controller tests were updated to the decide-then-approve flow.

**Notes for Testing (human eyes):**
- Decide + approve everything on a pair → Commit pair → confirm → the pair leaves the queue; re-add the same scenario → everything reads Same (hidden by default — enable the Same filter to see it).
- Open the kitchen sink: left and right panes now show the same expansion shape (remote-only dirs are struck on the left but expand in lock-step). "Synchronize view" normalizes both panes to the left pane's state.
- The top-right legend names the colors: blue Modified, green New, red Blocked, gray Same, struck gray Missing. Filter-row buttons (Show changes / Apply / Synchronize view / Close) should read clearly in both color schemas — please check your dark theme.
- Blocker rows (notes/A.txt, misc/notes.txt) accept the arrows; tooltips and the notes line state the exact automatic rename.
<!-- SECTION:FINAL_SUMMARY:END -->
