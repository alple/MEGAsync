---
id: MEGA-2.12
title: Stage 2 Testing feedback round 3
status: Testing
assignee: []
created_date: '2026-09-27 12:34'
updated_date: '2026-09-27 14:05'
labels:
  - sync-preview
milestone: Sync pre-commit review
dependencies: []
parent_task_id: MEGA-2
priority: high
type: enhancement
ordinal: 35000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Third round of MEGA-2.10/2.11 Testing feedback (2026-09-27, human tester), tracked as ONE new ticket per the tester's call.

**1. Peer review window does not open automatically.** Root cause verified: the tested binary (built 2026-09-26 20:46) predates the final commit (22f5ce4, 21:54) that added the `--sync-preview` hook — the flag work was step 8 of MEGA-2.11's plan, done after that build. Code is already correct in HEAD; fix = rebuild.

**2. Filter checkboxes barely visible on white.** The Same/Different/New checkboxes are plain QCheckBox without the app's `type="mega"` property, so the prod themed indicator images never apply and the native fallback washes out on the page background. Fix: set `type="mega"` so the prod checkbox look (per-schema images) applies.

**3. Narrow middle decision column.** A third, very narrow pane between the local/remote trees: per-row compact checkable arrow buttons `→` (local→remote), `←` (remote→local), `↔` (best-effort) — the same functionality as the bottom detail panel's buttons, one click per row without selecting. Semantics identical to the bottom panel (decide+approve in one gesture; un-click = explicit do-nothing; recommended action pre-clicked on unflagged rows; flagged rows start unclicked; ↔ disabled on blockers). Expansion/scroll/zebra mirrored across all three trees. Bottom action panel stays as is (tester: keep buttons too).

**4. Consequences popup replaced by a footer line.** The "Consequences of the directory action" modal is too much. Directory actions decide immediately on click; a one-line warning/info at the bottom of the window summarizes the consequences ("Directory action on <path>: …"), full path breakdown in the tooltip (tester-approved: one line + tooltip). Popup removed.

**5. Show-changes: per-row warning triangles.** Rows whose plan carries warnings get the exclamation-triangle icon (existing `alert-triangle-small` asset) on the path cell; hovering shows that path's warnings. The aggregated bullet list at the bottom is removed (tester-approved: triangle only). Warnings of subtree-covered rows fold into their covering directory row's tooltip so nothing is lost.

**6. Root rows spread out.** Root-level rows get extra top spacing (~+8px) and bold names so each root directory reads clearly as its own group; identical on all three trees (rows stay line-locked; `uniformRowHeights` must go off for per-row heights).

**7. Upstream check (2026-09-27): no new version.** Upstream master tip = 22e72f5ac (2026-09-21) = the fork's exact base; latest upstream release tag v6.6.2.0 matches the fork's prod version. Nothing to merge; recorded for the record.

Scope: sync_preview/ module + its .ui + Version.h bump (-dev.2 → -dev.3) + rebuild. Fake data only; nothing upstream-touched.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 #1 After a fresh `just build`, `just run` auto-opens the review window via the fork-only --sync-preview flag (the previously tested binary predated the flag commit — verified stale, code already in HEAD)
- [ ] #2 #2 The filter-row checkboxes (Same/Different/New) render the prod themed checkbox indicator (type="mega") and are clearly visible in both color schemas
- [ ] #3 #3 A narrow middle decision column shows per-row →/←/↔ checkable buttons with the exact decide+approve+un-decide semantics of the bottom panel (recommended action pre-clicked on unflagged rows, flagged rows unclicked, ↔ disabled on blockers); expansion, scrolling and zebra mirrored across all three trees; the bottom action panel is kept unchanged (buttons included)
- [ ] #4 #4 The consequences popup is gone: directory actions decide immediately on click and a one-line consequences note (warning color when anything is removed/overwritten, info otherwise) appears at the bottom of the window with the full path breakdown in its tooltip; the note clears when the decision is un-clicked, on Apply, and on a newer directory action
- [ ] #5 #5 In the Show-changes window every listed row carrying warnings shows the exclamation-triangle icon (alert-triangle-small) on its path cell with that path's warnings in the hover tooltip; warnings of subtree-covered rows fold into their covering directory row's tooltip; the aggregated bottom warning list is removed
- [ ] #6 #6 Root-level rows are visually spread out: extra top spacing (~+8px) and bold names on root rows, identical on all three trees so the rows stay line-locked
- [ ] #7 #7 Upstream checked (2026-09-27): no new version — upstream master tip equals the fork base (22e72f5ac, 2026-09-21) and the latest upstream release tag v6.6.2.0 matches the fork's prod version; recorded, nothing to merge
- [ ] #8 #8 VER_FORK_SUFFIX bumped -dev.2 → -dev.3; `just build` and `just test` green; diff confined to sync_preview/ (+ its .ui), Version.h; fake data only, nothing upstream-touched
<!-- AC:END -->

## Comments

<!-- COMMENTS:BEGIN -->
created: 2026-09-27 13:53
---
Round-2 fixes (2026-09-27, tester feedback after first Testing build) — build + tests green (939/136):

**Bold/margin didn't read as grouping** → real groups now: a blank 12px separator row is inserted above every root but the first, in all three trees (so the stripes stay aligned); root rows are bold on a subtle `surface-1` band (both panes + middle column, the middle's row widget made transparent so the band reads through). The old +8px size-hint-only spacing was too subtle — replaced by the visible gap.

**Middle column free space on the right** → column narrowed 110→96px, `setIndentation(0)` (buttons start at the cell edge), buttons stretch in the row layout with tighter padding and 10px font so the three of them fill the width.

**Rows didn't align across the three columns** → root cause: the middle tree's per-row button widgets sized themselves independently of the item size hints, making some middle rows taller. Row widgets are now `setFixedHeight` to exactly the row height (26px normal / 34px root), and the group-gap rows are identical in all three trees.
---

created: 2026-09-27 14:05
---
Round-3 fixes (2026-09-27, tester feedback after round-2 build) — build + tests green (939/136):

**Stripe mess → groups reverted.** Root cause of the desync was real: the middle column's ROOT rows were 34px tall (root extra height) while the panes' root rows stayed 26px, so every root row shifted the middle stripes 8px further. Per the tester's call the grouping experiment is gone entirely: no gap rows, no bold, no surface band — every row in all three trees is a uniform 26px again, stripes in step by construction.

**Middle column width coherence.** Root cause: `setFixedWidth` loses to QSplitter's initial size distribution — the splitter laid the column out wide from the tree's content size hint, and only after a manual resize did the fixed width clamp it (hence "snaps back lean"). Fix: min = max = 96px on the decision tree (coherent at every moment, resize can't move it) plus an explicit `splitter->setSizes({1, 96, 1})` on a zero-timer after the first layout pass, so the initial show is also lean.

Note: with grouping reverted, the tester's original request 6 (visible root separation) is intentionally NOT satisfied — uniform rows per tester's "let's not do the groups". If wanted later, a safer approach (selection-independent, stripe-safe) can be discussed.
---
<!-- COMMENTS:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Implemented MEGA-2.12 (third testing round, one ticket per the tester's call). `just build` clean, `just test` green: **939 assertions / 136 cases** (unchanged — all changes are UI-side, the controller/model seam untouched). Binary rebuilt fresh (the round's item 1 was a stale binary). Diff confined to `sync_preview/`, its `.ui`, `syncpreview.cmake`, the regenerated `translation.source.ts` and the `Version.h` bump. Fake data only, nothing upstream-touched.

**1. Auto-open (AC#1) — no code change; root cause was a stale binary.** The tester's binary was built 2026-09-26 20:46, but the `--sync-preview` hook landed in the final MEGA-2.11 commit 22f5ce4 at 21:54 (the flag work was step 8 of that plan, done after the last build). Verified via git history: `mAutoOpenSyncPreview` does not exist pre-22f5ce4, and the `sync-preview` string hit in the old binary is the queue filename. The fresh build (post-change, verified by mtime) picks the hook up: `just run` auto-opens the review window.

**2. Checkbox visibility (AC#2).** The Same/Different/New checkboxes now carry the app's `type="mega"` property, so the standard sheet's themed indicator images (checkbox_on/off, per schema) apply instead of the washed-out native fallback. Token text colors kept (existing AC#5 path).

**3. Middle decision column (AC#3).** A third narrow tree (`decisionTree`, fixed 110px, headerless, NoFocus/NoSelection) sits between the panes in the splitter (stretch 1:0:1). Every row gets a row-widget with three compact checkable arrows — `→`, `←`, `↔` — through the SAME decide+approve path as the bottom panel (`onActionButtonClicked(path, choice, checked)` shared by both surfaces): click = decide + approve, click again = explicit do-nothing, recommended action pre-clicked on unflagged rows, flagged rows start unclicked, `↔` disabled on blockers with the explanatory tooltip. Buttons work per row without selecting. All three trees are line-locked: zebra striping, expansion mirroring (the column follows the shared map even when Synchronize view is OFF — it has no free state) and scrolling (the column always follows the left pane). The bottom action panel stays, buttons included (tester's call). `GuiStyle::actionButtonStyleSheet(compact)` gained a tight-padding variant so three buttons fit a row.

**4. Consequences popup replaced by a footer note (AC#4).** `SyncPreviewConsequencesDialog` deleted (files + cmake + include). Directory actions now land immediately on click; a one-line note appears in the footer ("Directory action on <path>: 1 removed locally, 2 overwritten on MEGA."), warning-colored when anything is removed/overwritten, quiet otherwise, full per-path breakdown in the tooltip (tester-approved shape). The note clears when the directory decision is un-clicked (explicit do-nothing), and is replaced by any newer directory action. Cancel-before-decide no longer exists — the gesture is the same instant decide the file rows already had.

**5. Per-row warning triangles in Show-changes (AC#5).** Each listed row whose plan carries warnings shows the exclamation triangle (`alert-triangle-small` + @2x, existing app resource) on its path cell; hovering shows that path's warnings. Warnings of subtree-covered rows fold into their covering directory row's tooltip, so nothing is lost with the aggregated bottom bullet list removed (tester-approved: triangle only). The awaiting-approval warning coloring is untouched.

**6. Root-row spacing (AC#6).** Root-level rows get +8px height and bold names on all three trees (per-row heights: `uniformRowHeights` off), so each root directory reads as its own group while the rows stay line-locked.

**7. Upstream check (AC#7) — no new version.** After `git fetch upstream --tags`: upstream master tip = 22e72f5ac (2026-09-21) which IS the fork's base commit; the latest upstream release tag v6.6.2.0 (all platforms, 2026-09-09) matches the fork's prod version. Nothing to merge.

**8. Version + verification (AC#8).** `VER_FORK_SUFFIX` → `-dev.3` per fork policy (new fork-side changes ship in a new dev number). `just build` and `just test` green; `git status` diff confined to the declared scope.

**Notes for Testing (human eyes):**
- `just run` → review window opens by itself (the previous "doesn't open" was the stale binary; this build is fresh).
- Filter checkboxes now have the prod check/box look in your dark schema — verify on white too (light schema was the complaint).
- Middle column: click an arrow on any row without selecting it — same decision flow as the bottom panel (directory rows land instantly now, no popup). Un-click = do nothing. `↔` greyed on blocker rows.
- Show changes: warning rows carry a triangle; hover it for that path's warnings (no more bottom list).
- Root directories are spaced out and bolded in all three columns.
<!-- SECTION:FINAL_SUMMARY:END -->
