---
id: MEGA-2.8
title: >-
  Stage 2 UI polish: meld-style folder-diff presentation in the pair detail
  window
status: Testing
assignee:
  - agent
created_date: '2026-09-24 18:49'
updated_date: '2026-09-24 19:23'
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
<!-- SECTION:FINAL_SUMMARY:END -->
