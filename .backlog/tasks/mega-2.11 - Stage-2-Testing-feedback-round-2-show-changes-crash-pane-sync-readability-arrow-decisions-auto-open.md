---
id: MEGA-2.11
title: >-
  Stage 2 Testing feedback round 2: show-changes crash, pane sync + readability,
  arrow decisions, auto-open
status: Testing
assignee:
  - '@agent'
created_date: '2026-09-26 18:20'
updated_date: '2026-09-26 18:48'
labels:
  - sync-preview
milestone: Sync pre-commit review
dependencies: []
parent_task_id: MEGA-2
priority: high
type: enhancement
ordinal: 34000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Second round of MEGA-2.10 Testing feedback (2026-09-26, human tester). The pair-review UI is functional but has one app-killing defect and several UX/readability gaps that block comfortable human review of the fake-data demo.

**1. App crash on closing the Show-changes popup (blocking).** Verified root cause: `SyncPreviewChangesDialog` and `SyncPreviewConsequencesDialog` are stack-allocated, `exec()`'d, and set `Qt::WA_DeleteOnClose` in their constructors. Qt5 schedules a deferred delete on close; deleting a stack object aborts with `free(): invalid size` (SIGABRT, reproduces the tester's `Recipe run terminated ... signal 6`). Minimal standalone Qt5 probe confirmed the mechanism. Fix: remove the attribute from both popups (they are parented stack objects; lifetime is scoped by `exec()`).

**2. The Show-changes popup is unreskinned.** Only its tree gets the palette treatment; the window itself, header/warning labels and the button box fall back to default colors — unreadable in the dark schema. It must read like the pair-detail window: window palette, token-colored labels, styled button box, re-resolved on theme change. Same treatment for the consequences popup (same gaps).

**3. Pane correspondence.** The tester reported left/right trees still reading inconsistently: the type-mismatch blocker row (`misc/notes.txt`: file locally, folder with `draft.txt` remotely) renders a file icon + no folder affordance on the left pane and a folder icon with visible children on the right, so the same row reads differently per pane. Panes are already line-locked (same rows, same order). Two fixes agreed with the tester:
   - Folder-ness of a row is pane-invariant: a row whose either side is a folder renders folder icon + expansion affordance on BOTH panes; the side whose actual entry mismatches keeps its strike-through/annotation and tooltip.
   - Zebra striping (alternating row tint from the existing `surface-1` token) on both panes so corresponding rows are easier to track across panes.

**4. Unreadable filter-row and summary text.** `Filter:` label, the Same/Different/New checkboxes and the footer summary label in `SyncPreviewPairDetailDialog.ui` carry no token styling and render dark-on-dark in the dark schema. Give them explicit token colors (same re-resolve-on-theme-change path as the rest of the chrome).

**5. Synchronize view must be a toggle, not a one-shot button.** Expansion/selection/scroll mirroring is currently unconditional; the tester wants to browse synchronously OR each pane freely. Checkable button, default ON: ON = continuous lock-step mirroring plus a normalize (mirror left expansion onto right) at enable time; OFF = panes browse independently.

**6. Apply belongs in the Show-changes window.** The popup gains an Apply action (executes the plan exactly as the main-window Apply does, closes the popup with Accepted so the detail window rebuilds) alongside Close. The main filter row's Apply button moves there and is removed from the filter row.

**7. Decision buttons become three toggle arrows.** Replace the four action buttons + separate Approve toggle with three checkable buttons: `→` (local→remote), `←` (remote→local), `<->` (best-effort/automatic). Behavior agreed with the tester:
   - The recommended action's button starts checked (the plan already applies recommendations for undecided rows — the UI state now shows that).
   - Ambiguous rows (conflict/blocker) start with no button checked; the row stays undecided/unapproved until the tester clicks one.
   - Clicking a button = decision AND approval in one gesture (commit gate semantics unchanged: ambiguous rows still block until clicked).
   - Unchecking the active button = "do nothing" (explicit None decision; the planner must not fall back to the recommendation for it).
   - The Do-nothing and Approve buttons disappear.
   - Exception (tester-approved): best-effort stays disabled on blocker rows — a merge cannot fix a structural collision; tooltip says so.
   - The prose description label to the right of the buttons keeps showing the currently clicked action's meaning (and the recommendation when nothing is clicked).
   - Directory rows still go through the consequences popup before the decision lands.
   - Nice MEGA arrow icons welcome if a suitable existing asset exists; text arrows are acceptable.

**8. Auto-open the review window on `just run`.** The tester must currently navigate menus after startup. Fork-side CLI flag `--sync-preview` (parsed beside the existing `--version`/`--debug` args in `MegaApplication`, behavior-additive and opt-in only — prod unaffected unless the flag is passed), which opens `showSyncPreviewDialog()` once startup settles. The justfile `run` recipe passes the flag by default. Fork version suffix bump `-dev.1` → `-dev.2` (fork policy: new fork-side changes ship in a new dev number).

Scope: `sync_preview/` module + its tests + justfile + the flag hook in `MegaApplication.cpp` + Version.h bump. Fake data only; nothing upstream-touched.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Closing the Show-changes popup and the directory consequences popup no longer aborts the app (no WA_DeleteOnClose on stack-exec'd dialogs)
- [ ] #2 The Show-changes popup and the consequences popup render with the same dark token styling as the pair-detail window, re-resolved on theme change; header and warning labels readable in both color schemas
- [ ] #3 A row whose either side is a folder shows folder icon + expansion affordance on BOTH panes (mismatched side struck through); the type-mismatch blocker row reads the same shape left and right
- [ ] #4 Both panes show zebra striping from the surface token, so corresponding rows are visually trackable across panes
- [ ] #5 Filter label, Same/Different/New checkboxes and the summary label are readable in both color schemas (token-colored)
- [ ] #6 Synchronize view is a checkable toggle defaulting to ON: ON keeps panes mirroring continuously (expansion/selection/scroll) and normalizes on enable; OFF allows independent pane browsing; re-enabling re-normalizes
- [ ] #7 Apply lives inside the Show-changes window (executes the plan, closes, panes rebuild); the main filter row has no Apply button
- [ ] #8 The action panel shows exactly three checkable buttons (→, ←, <->): recommended action pre-clicked, ambiguous rows unclicked; clicking decides + approves in one gesture; unchecking records an explicit do-nothing that the planner does not override; Do-nothing and Approve buttons are gone; best-effort stays disabled on blocker rows with an explanatory tooltip
- [ ] #9 just run launches the app with the review window auto-opened via the fork-side --sync-preview flag; without the flag prod behavior is unchanged
- [ ] #10 just build and just test green; controller tests updated for the unified decide+approve flow; diff confined to sync_preview/, tests, justfile, the flag hook in MegaApplication.cpp and the Version.h bump (-dev.2)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
## Implementation plan (recorded before implementation, 2026-09-26)

All research done pre-plan: crash mechanism empirically confirmed with a standalone Qt5 probe (/tmp/kilo/qdialog-crash-probe: stack QDialog + WA_DeleteOnClose + exec() + close = `free(): invalid pointer`, SIGABRT). Fake data only; scope = sync_preview/ + tests + justfile + MegaApplication.cpp flag hook + Version.h bump.

### 1. Crash fix (AC#1) — `SyncPreviewChangesDialog.cpp` + `SyncPreviewConsequencesDialog.cpp`
- Remove `setAttribute(Qt::WA_DeleteOnClose)` from both constructors (stack+exec'd; lifetime scoped by exec()).

### 2. Reskin both popups (AC#2) + Apply inside Show-changes (AC#7)
- `SyncPreviewChangesDialog`: `GuiStyle::applyWindowPalette(this)`; header + notes labels token-colored (text-primary / text-warning); QDialogButtonBox buttons get `GuiStyle::actionButtonStyleSheet()`; new `applyPalette()` member re-resolving on ThemeManager::themeChanged (register like the detail dialog does).
- New: dialog gains an **Apply** button (QPushButton, "Apply") beside Close: clicking runs a callback into the detail window (or emits accepted): detail window's `showChanges()` handles Accepted → `applyPlan()`; Rejected → nothing. Consequences popup gets the same palette treatment (labels + button box).
- `.ui` + detail dialog: remove `applyButton` from the filter row (property + connect + chrome list). Show-changes popup tooltip on Apply mirrors the old filter-row one.

### 3. Pane correspondence (AC#3, AC#4) — `SyncPreviewPairDetailDialog.cpp`
- `makeSideItem`: folder-ness pane-invariant — `const bool rowIsFolder = row.local?.isFolder() || row.remote?.isFolder()` decides icon + expansion on BOTH panes (side file-vs-folder mismatch keeps its own size/time text and strike/annotation via PaneState::Blocked; tooltip already explains).
- Zebra: `tree->setAlternatingRowColors(true)` + AlternateBase already token-colored in `applyViewPalette` (surface-1). Verify tint renders in both schemas; both panes line-locked so stripes correspond.

### 4. Chrome readability (AC#5)
- `applyPanesPalette`: token-colored stylesheet for `filterLabel`, the 3 checkboxes, `summaryLabel`, `pairLabel` (text-primary), re-resolved on theme change (same proven per-widget sheet path as the chrome buttons).

### 5. Synchronize view toggle (AC#6)
- `synchronizeViewButton` becomes checkable, `setChecked(true)` at setup; a `mPanesLocked` flag replaces unconditional mirroring: `syncScrollFrom`/`syncSelectionFrom`/`syncExpansionFrom` early-return when OFF (but still record expansion into `mExpandedByPath` so rebuilds don't fight the free state).
- Toggling ON runs the old normalize (mirror left expansion onto right + re-apply selection/scroll); toggling OFF does nothing further.
- On rebuild: restore expansion per pane from `mExpandedByPath` on both panes (unchanged); restore selection on both; scroll position only re-applied when ON (OFF = independent scroll).

### 6. Arrow decision buttons (AC#8)
- `actionChoices()` drops `Action::None`; `actionButtonText`: `<-`, `->`, `<->`; tooltips updated ("decide + approve in one; click again to un-decide").
- Button group non-exclusive; custom click handling: click checked button → uncheck + `setAction(None)` (explicit do-nothing) ; click unchecked → check it (uncheck others manually), consequences popup first for directory rows (existing), then `setAction(action)` + `setApproved(true)` in one gesture (extend controller call sequence in the dialog; keep controller API).
- `updateActionPanel`: checked state = decision action if present, else recommended action IF the row is unambiguous (not conflict/blocker); ambiguous → none checked. Best-effort disabled on blockers (existing) with tooltip. Approve button removed. Description label shows the effective/clicked action's prose + recommendation hint.
- Controller: verify `setAction(None)` records an explicit None decision the planner does not fold back into the recommendation (read Planner; add a test pinning this).
- Tests: update controller tests to the decide+approve-unified flow; new tests: uncheck → explicit None survives planning; recommended pre-selection does not auto-approve ambiguous rows; commit gate still blocks unclicked ambiguous rows.

### 7. Auto-open + version (AC#9)
- `MegaApplication.cpp`: parse `--sync-preview` beside `--debug`; after startup settles (where the menu/tray is functional) schedule `QTimer::singleShot(0, ...)` → `showSyncPreviewDialog()` when the flag was passed. ~10 lines, flag-gated, opt-in only.
- justfile `run`: prepend `--sync-preview` (keep `{{args}}` forwarding); comment the flag.
- `Version.h`: `VER_FORK_SUFFIX` `-dev.1` → `-dev.2`.

### Tests & verification
- `just build` + `just test` (922 assertions baseline) green.
- Manual crash check: probe app pattern is fixed by construction (no DeleteOnClose on stack dialogs); grep sweep for remaining `WA_DeleteOnClose` in sync_preview exec()'d popups.
- Fake-data UI walk: kitchen-sink pair — type-mismatch row same shape both panes; zebra visible; popup Apply works; arrows toggle semantics; sync-view toggle free/locked browsing.

### Order
Crash fix → popup reskin + Apply-in-popup → pane folder-invariance + zebra → chrome colors → sync toggle → arrow buttons + controller tests → flag + justfile + version → build + test → finish (Testing + summary).
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Implemented MEGA-2.11 on top of the MEGA-2.10 state. `just build` clean, `just test` green: **939 assertions / 136 cases** (was 922/135). Diff confined to `sync_preview/`, its tests, the regenerated `translation.source.ts`, `justfile`, the flag hook in `MegaApplication.cpp/.h` and the `Version.h` bump. Fake data only.

**1. Crash fixed and empirically verified (AC#1).** Root cause: `SyncPreviewChangesDialog` and `SyncPreviewConsequencesDialog` are stack-allocated + `exec()`'d but set `Qt::WA_DeleteOnClose`; Qt schedules a deferred delete of a stack object on close → `free(): invalid size` / SIGABRT (the tester's `signal 6`). Reproduced in a standalone Qt5 probe (abort with the attribute, clean exit without), then removed the attribute from both popups. The remaining `WA_DeleteOnClose` uses (pair list, pair detail) are heap-allocated `show()` windows — correct pattern, untouched.

**2. Both popups reskinned (AC#2).** Window palette, token-colored header/empty/warning labels, tree on the page background with zebra rows, button box in the proven quiet chrome sheet — all re-resolved on `ThemeManager::themeChanged` (both popups register for theming as before).

**3. Apply moved into the Show-changes window (AC#7).** The popup gained an Apply button (accepts → the caller runs the review-loop apply: detail window calls `applyPlan()`, the pair list calls `controller->applyPlan(pairId)`); the filter-row Apply button is gone from the `.ui` and the detail window. Caught in review: `QDialogButtonBox::Apply` only emits `clicked()`, never `accepted()` — wired directly, else Apply would have been a no-op.

**4. Pane correspondence (AC#3, AC#4).** `makeSideItem` now keys the icon + expansion affordance on the ROW's kind (`rowIsFolder` = either side is a folder), not the side's entry: the type-mismatch blocker (`misc/notes.txt` file locally, folder remotely) renders folder icon + expansion on BOTH panes; the mismatching side keeps its blocked color and the explanatory tooltip. Both panes zebra-stripe from the `surface-1` token (line-locked rows, so stripes correspond).

**5. Chrome readability (AC#5).** `pairLabel`, `filterLabel`, `summaryLabel` and the three state checkboxes pinned to `text-primary` in `applyPanesPalette`, re-resolved on theme change (same proven per-widget-sheet path as the chrome buttons).

**6. Synchronize view is a toggle (AC#6), default ON.** ON = continuous lock-step (expansion/selection/scroll mirroring, one shared reference map); OFF = each pane browses freely with its OWN expansion map + selection memory, so rebuilds (decisions, filters, theme changes) never snap a free pane back; scroll restores per-pane when OFF. Re-locking normalizes (left expansion → right, selection re-anchors left, scroll re-aligns) and replaces the right pane's free map.

**7. Arrow decision buttons (AC#8).** Three checkable buttons: `→`, `←`, `↔` (best-effort). Unflagged rows: the effective action (own decision / inherited / recommendation) pre-clicked. Flagged rows (conflict, blocker, both-differ): nothing pre-clicked — the click IS the approval; `setAction` + `setApproved(true)` in one gesture. Un-clicking the active arrow records an explicit do-nothing (`setAction(None)` + `setApproved(false)`) the planner respects (pinned by test). The Do-nothing and Approve buttons are gone; the description right of the buttons shows the clicked action's prose, or "Needs your decision" (+ recommendation when one exists), or the inherited-from-directory note. Best-effort stays disabled on blocker rows (tester-approved exception; tooltip explains). Directory rows still pass the consequences popup; canceling leaves state untouched.

**8. Auto-open + version (AC#9).** Fork-only `--sync-preview` CLI flag parsed beside `--debug`; `start()` schedules `showSyncPreviewDialog()` once startup settles (idempotent: DialogOpener reuses an open window). `just run` passes the flag; without it prod behavior is unchanged. `VER_FORK_SUFFIX` → `-dev.2` per fork policy.

**Notes for Testing (human eyes):**
- `just run` → the review window opens by itself.
- Show-changes popup: readable dark theme, Apply applies (panes rebuild), Close no longer crashes the app; consequences popup same treatment.
- Kitchen sink: `misc/notes.txt` reads as a folder on BOTH panes (left side keeps blocked red + tooltip), zebra rows on both panes.
- Filter labels/checkboxes and the summary line readable in your dark theme.
- Arrows: unflagged rows show their action pre-clicked; conflicts/blockers/both-differ rows start unclicked and block commit until clicked; click again to un-decide (do nothing); `Synchronize view` unchecks for free pane browsing and re-checks to re-lock.
<!-- SECTION:FINAL_SUMMARY:END -->
