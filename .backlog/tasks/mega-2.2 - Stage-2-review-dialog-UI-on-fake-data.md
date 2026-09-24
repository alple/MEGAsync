---
id: MEGA-2.2
title: 'Stage 2: review dialog UI on fake data'
status: To Do
assignee: []
created_date: '2026-09-24 11:29'
updated_date: '2026-09-24 11:29'
labels:
  - sync-preview
milestone: Sync pre-commit review
dependencies:
  - MEGA-2.1
parent_task_id: MEGA-2
type: feature
ordinal: 25000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Build the review dialog UI (standalone widget dialog, StalledIssuesDialog module pattern — all-new files, per MEGA-2 UI decision) running entirely on the Stage-1 fake provider: dual-pane rows (local left / remote right), per-row and per-directory action choice (L→R / R→L / best-effort) with recommended action marked and overridable, conflict/blocker flags with explicit approval, commit gated until all rows approved, directory-action consequences popup, identical-rows hidden by default with toggle, row-count cap with load-more, multi-pair queue (add/remove pairs, filterable list grouped by pair), and restore of the persisted pair queue on reopen. No real API integration in this stage.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Dual-pane per-pair view (local left, remote right) listing all classified rows with per-side size and modified date and a newer-side marker
- [ ] #2 Per-row and per-directory action selection among L→R / R→L / best-effort, with the recommended action pre-marked and everything overridable
- [ ] #3 Conflicts and blocker rows are visually flagged and require explicit approval; commit stays disabled until all rows are approved
- [ ] #4 Directory-level actions trigger a consequences popup listing which files are removed and which changed
- [ ] #5 Identical rows hidden by default with an 'in sync' toggle; huge lists capped with load-more
- [ ] #6 Multi-pair queue: several candidate pairs can be added, reviewed, and removed; list filterable, grouped by pair by default
- [ ] #7 Runs entirely on the fake provider; no real API calls
- [ ] #8 Closing and reopening the dialog restores the persisted pair queue and decisions; changed classifications are re-flagged; completed pairs are dropped
<!-- AC:END -->
