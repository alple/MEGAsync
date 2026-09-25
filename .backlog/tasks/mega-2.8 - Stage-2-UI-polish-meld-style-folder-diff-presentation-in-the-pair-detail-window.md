---
id: MEGA-2.8
title: >-
  Stage 2 UI polish: meld-style folder-diff presentation in the pair detail
  window
status: Testing
assignee:
  - agent
created_date: '2026-09-24 18:49'
updated_date: '2026-09-25 16:10'
labels:
  - sync-preview
milestone: Sync pre-commit review
dependencies:
  - MEGA-2.7
parent_task_id: MEGA-2
priority: high
type: enhancement
ordinal: 31000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up from MEGA-2.7 Testing feedback (2026-09-24): the two-pane detail window works but reads "ugly and not usable"; user asked to "integrate some tree comparator, like meld".

**Direction decided (user picked):** port meld's folder-comparison presentation INTO the existing dual-pane detail window. Not viable alternatives, ruled out: embedding meld itself (GTK/Python, GPLv2 — incompatible with this proprietary Qt fork tracking upstream, heavy merge tax) and launching meld as an external process (remote side is cloud data with no local folder — real pairs would need full remote materialization, a Stage 3+ decision; meld has no notion of our action/approval decisions, so the review flow would split across two apps).

**Meld semantics to port (verified against meld's folder-mode docs):** per-side states — Same (normal font), Modified (blue + bold), New (green + bold), Missing (grayed strikethrough), Error (bright red); state colors EACH PANE per side, so one row can read "New" on the left pane and "Missing" on the right; state filters as Same/Different/New checkboxes with Same rows hidden by default. Our current shared per-row coloring is part of why the view reads wrong.

**Mapping to our RowKind (Stage-2 classification):** Identical → Same (both panes, dimmed when shown); BothDiffer → Modified (both, blue bold); LocalOnly / RemoteOnly / Conflict (per-side rows) → New on the side that exists + Missing on the other; Blocker → Blocked (both, bright red), always visible. Newer-side marker and re-flag keep as compact status suffixes / middle-strip label; details stay in tooltips.

**Durable facts gathered:** theme tokens exist for the states — text-success (#007c3e light / #09bf5b dark), text-info (#0078a4 / #05baf1), text-error (#e31b57 / #fd6f90), text-secondary (#616366 / #a9abad); surfaces/palettes from MEGA-2.7 carry over (page-background, surface-1, surface-inverse-accent). Candidate icon resources for entry types: images/node_selector/search_filter/small_folder_default.png (folder) and images/themed/common/MIME/generic_small.svg (qrc alias generic_small; gradient-filled, not color-tokenized — verify dark-theme aliasing before reuse, else skip icons).

**Context to keep from MEGA-2.7 (already in place):** pair-first list dialog; SyncPreviewPairDetailDialog QSplitter layout (left local tree | middle action strip | right remote tree), row lock-step across all three views (same row index, uniform 40px rows, scroll+selection sync), path filter, load-more cap, consequences popup flow, theming registration + rebuild on theme change, non-modal one-window-per-pair.

**Scope:** Stage 2, fake data only; no model/controller changes; Stage-5 touch points untouched.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Per-pane state coloring follows meld's folder-diff semantics, per SIDE not per row: entry exists on this side only → New (green bold); paired content differs → Modified (blue bold); side lacks the entry → Missing (gray strikethrough); identical → Same (normal, de-emphasized); blocker → Blocked (bright red bold) on both panes
- [ ] #2 Status column per pane with state text (New / Modified / Same / Missing / Blocked); bold for New/Modified/Blocked, strikethrough for Missing; colors from theme tokens only (text-success / text-info / text-error / text-secondary / text-primary) with sufficient contrast in BOTH light and dark
- [ ] #3 Meld-style filter checkboxes in the detail window: Same / Different / New — Same unchecked by default (identical rows hidden, preserving MEGA-2.7 AC#3), Different and New checked, Blocker rows always visible regardless; the lone Show-in-sync toggle is replaced
- [ ] #4 Entry-type icons appear in both panes reusing existing app resources, with no new image assets added; if themed rendering of those resources is wrong per theme, skip icons and note the limitation
- [ ] #5 Scope guard: no model/controller changes; fake data only (Stage 2); lock-step panes, path filter, load-more cap, consequences popup, commit gating, multi-pair add/remove preserved; no real API calls
- [ ] #6 just build compiles clean and just test stays green
<!-- AC:END -->

## Comments

<!-- COMMENTS:BEGIN -->
created: 2026-09-24 19:55
---
Testing rejection (2026-09-24): per-side recolor of the MC 3-pane layout "ain't it" — the meld-true target is the actual meld folder-diff anatomy (two row-locked expandable trees + thin state-bars column, actions out of the middle). Direction re-decided with the dev: meld-true layout, actions move to a bottom panel (vs drawer/context-menu alternatives), real expandable trees with synced expansion. Research verdict recorded: no C++ folder-diff view exists that this proprietary fork can lift (KDiff3/WinMerge/FreeFileSync/Kompare/meld are all GPL; SDK exposes no tree diff) — the look must be our own code, behavior-porting only. A throwaway Qt prototype with three layout variants (A bars+bottom panel, B bars+side drawer, C no-bars color-only) on the real fake-data engine is at /tmp/kilo/meld-proto (run.sh); variant pick will drive the real dialog rework. Ticket stays Testing until the layout verdict lands.
---
<!-- COMMENTS:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Implemented meld-style folder-diff presentation in the pair detail window (Stage 2, fake data only). Build green (`just build`), tests green (`just test`: 646 assertions / 108 cases). Diff = 3 gui files in sync_preview/gui only (no model/controller changes).

**AC#1 — per-side states:** file-local `PaneState` (Same/Modified/New/Missing/Blocked) + `stateFor(row, side)`: Identical→Same (both), BothDiffer→Modified (both), LocalOnly/RemoteOnly/Conflict→New on the existing side + Missing on the other (classifier keeps Conflict rows one-sided), Blocker→Blocked on BOTH panes. Colors per pane from tokens only: New=text-success, Modified=text-info, Blocked=text-error, Same/Missing=text-secondary. Bold New/Modified/Blocked, strikethrough Missing (path + status columns; size/modified keep plain font, take state color). Replaced the MEGA-2.7 shared per-row color + `[CONFLICT/BLOCKER…]` badge prefix.

**AC#2 — Status column:** added to both panes (Path | Status | Size | Modified); status = state word + compact "newer: local/remote" suffix for two-sided file rows (equal timestamps no longer spell "same"; the old re-approve warning text stays only on the action strip's label).

**AC#3 — state filters:** `showInSyncToggle` replaced by Same (unchecked by default) / Different / New checkboxes. Bucketing: Same=Identical, Different=BothDiffer, New=LocalOnly/RemoteOnly/Conflict (meld semantics: each conflict side's name exists on one side only — conflict rows hide if New is unchecked). Blocker rows always visible.

**AC#4 — icons:** folder `:/images/node_selector/search_filter/small_folder_default.png` (+@2x for HiDPI), file `:/images/themed/common/MIME/generic_small.svg`; plain QIcon from qrc, no new assets. MISSING-side cells show "—" gray-struck with no icon.

**AC#5 — preserved:** row lock-step, scroll/selection sync, path filter, load-more cap, consequences popup, commit gating, multi-pair add/remove, theming rebuild on theme change. No model/controller changes.

Notes for Testing (human eyes needed):
- Icon dark-theme aliasing is unverified here (can't launch GUI against prod data dir): if generic_small.svg aliases badly in dark theme, AC#4 says skip icons.
- Icons left-align in the path column while text stays space-indented (flat-icon look); meld indents icons with the tree — a delegate could fix later if it bothers.
- Status column is fixed 140px so "Modified · newer: remote" fits; path column lost ~145px of stretch vs MEGA-2.7 at min window width.
- Deliberate drop: MEGA-2.7's de-emphasis of cascade-covered rows (`coveredByPath`) — the meld mapping colors strictly by RowKind/side; covered rows still show their effective (parent-driven) action in the strip.
- Pre-existing `setFirstItemColumnSpanned` deprecation warning (from MEGA-2.7's load-more row) left as-is — not this ticket's churn.

Note: MEGA-2.7's baseline was committed as 534ec2c mid-session (not by this session); this diff sits cleanly on top of it, uncommitted as agreed.

REWORK per Testing verdict (2026-09-25, prototype variant C chosen): the pair detail window is now meld-true. The 380px middle action strip is GONE — two expandable QTreeWidgets only (left local, right remote), row-locked by PATH: expansion, selection and scrolling sync across panes by path key (not row index), so the trees stay aligned like meld's folder comparison. Rows are real hierarchy (QTreeWidget nesting, no more space-indent hacks) at a meld-dense 26px, folders default-expanded with expansion state surviving rebuilds. Status column dropped (color-only states, per variant C): per-pane state colors/fonts identical to the previous iteration (Same dimmed / Modified blue bold / New green bold / Missing struck / Blocked red both panes). AC#2's status column is superseded by the verdict — state now reads via color+font and the action panel's per-side state line.

Actions moved to a bottom panel under the trees (built in-code, QFrame): line 1 = selected path (bold) + per-side states in state colors; line 2 = four pressable exclusive action buttons [L→R] [R→L] [Best-effort] [Do nothing] + Approve toggle + right-aligned 'Recommended: X' hint; line 3 = notes (re-approve / conflict / blocker / twin) in warning/error token color. Effective action (planner-decided) is the pressed button; blockers disable the action buttons; re-click on the active action is a no-op (no consequences-popup spam). SyncPreviewRowWidget retired (deleted; its only consumer was the mid strip) — syncpreview.cmake updated, translations regenerated. Nothing else consumes it.

Tree population: classification rows grouped by parentPath (every folder path has a row — verified in the classifier), sorted case-insensitive per level; state-filtered rows hidden unless they have visible descendants (structural ancestors keep their true state colors so the tree stays connected); path filter unchanged; load-more cap preserved (button row appended under BOTH panes, line-locked); expansion state kept across rebuilds in a path→expanded map. Selection by path restores across filter edits; filtered-away selection resets the panel. Consequences popup flow, commit gating, multi-pair add/remove, theming rebuild on theme change, non-modal windows all preserved. No model/controller changes (classifier's folder subtree verdicts drive directory states). Build clean, just test green (646/108). RowWidget's stale .ts context dropped by the generate_ts run.

For Testing: (1) expand/collapse a folder on ONE side — the sibling pane must mirror it (meld behavior); (2) folder states: a folder containing any change reads Modified (classifier subtree verdict), untouched folders read Same and are hidden unless Same is checked; (3) the action panel on a selected directory row still routes through the consequences popup (cancel must restore); (4) button arrangement — the panel is two labeled lines (path+states, buttons+approve+hint) — react to the arrangement; (5) Same-filter structural ancestors render in their real state color — check they don't distract; (6) icon dark-theme aliasing check still pending from the previous round.

FEEDBACK ROUND 2 fixes (2026-09-25): (1) Taskbar/window icon — both review windows set the MEGA app icon (:/images/app_ico.ico) explicitly; the app-level icon doesn't reliably reach these non-modal windows under Wayland. (2) Selection read glaringly bright (surface-inverse-accent fill) — both windows' views now select with the translucent neutral-container-hover token + text-primary, a quiet grey-on-dark highlight. (3) Blocker rows used to disable ALL action buttons incl. Do nothing (unreadable grey + approvable no-op) — 'Do nothing' is now always clickable (it IS the no-op state), only the transfer buttons stay disabled-but-readable with 'arrives in a later stage' tooltips, and the Approve toggle is hidden when the effective action is None (approving a do-nothing was meaningless; engine gating reconciliation moved to MEGA-2.9). (4) Action buttons labeled 'L→R'/'R→L' re-read as confusing word pairs — button faces are now '→' and '←' (left pane stays left, right pane right, only the arrow flips), with descriptive tooltips (Transfer local → remote etc.) and the recommendation hint spelled in prose ('Recommended: local → remote').

Fake data: kitchen sink gained a 4-level nested branch (projects/mega/client/src) with identical top and a CRC-only modification at the bottom so folder paths read Modified down a real hierarchy. New canned scenario 'renameSwap' pins the moved-content trap the user called out (local foo.txt@hash_1 | remote foo.txt@hash_2 + remote bar.txt@hash_1 — naive L→R would overwrite content the remote already keeps under bar.txt); pickable in the pair picker as 'Demo: rename swap (content moved to a new name)' and pinned by a new unit test (652 assertions / 109 cases).

Moved OUT of this ticket into new MEGA-2.9 (Backlog, depends on this): rename-aware transfer semantics for conflict/blocker rows, approval-gating reconciliation in the engine, twin detection for PAIRED rows (rename-swap resolution), the Show-changes/Apply review loop on fake data, and further scenario fishing — per the user's own call that this exceeds the current ticket's context.
<!-- SECTION:FINAL_SUMMARY:END -->
