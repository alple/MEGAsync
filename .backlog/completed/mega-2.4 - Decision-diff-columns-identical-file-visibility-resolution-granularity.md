---
id: MEGA-2.4
title: 'Decision: diff columns, identical-file visibility, resolution granularity'
status: Done
assignee: []
created_date: '2026-09-24 10:32'
updated_date: '2026-09-24 11:05'
labels:
  - 'wayfinder:grilling'
milestone: Sync pre-commit review
dependencies: []
parent_task_id: MEGA-2
type: task
ordinal: 6000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Question

What does a diff row show and how are resolutions expressed?

User's stated analogy: GitHub diff — file changes, which side is newer, date and size (most files will be binary, so no content diffing). Established direction: the list shows what would transfer up and what would transfer down; conflict rows show both sides.

Decide: exact row schema (name, relative path, per-side size + modified date, direction badge, newer-side marker); whether identical files are hidden by default (recommended: hidden with an "in sync" toggle); resolution granularity — per-file choice plus per-directory bulk decision ("this folder: local wins" cascades to all differing files under it) plus global "all local wins"/"all remote wins" (user's latest answer leans file+dir+global; confirm); presentation of conflict rows (candidate for reuse: the dual-card compare widget LocalAndRemoteChooseWidget in stalled_issues/gui/stalled_issues_cases/, already showing size/mtime/ctime/CRC/versions per side).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A resolution comment records the final row schema, visibility default, granularity model, and sort/filter set
- [ ] #2 The schema covers: direction (up/down/both-differ), per-side size and modified date, newer-side marker, and relative-path presentation
- [ ] #3 Granularity covers per-file, per-directory bulk, and global actions, with cascade semantics for directory-level choices stated explicitly
- [ ] #4 Identical-file visibility default is stated (hidden-by-default with a toggle is the current recommendation)
<!-- AC:END -->

## Comments

<!-- COMMENTS:BEGIN -->
created: 2026-09-24 11:05
---
Resolution (2026-09-24, user-defined model supersedes the original column question): **dual-pane review** — local on the left, remote on the right. Per file AND per directory, three actions: **make remote like local (L→R)**, **make local like remote (R→L)**, **best-effort (both-way merge)**. A recommended action may be marked but every row stays user-overridable — the app never decides on its own. Conflicts (same name on both sides, including same-content-different-name) are flagged and require explicit approval; commit is blocked until all rows are approved. Directory-level actions show a consequences popup (which files removed, which changed). Identical rows hidden by default with an 'in sync' toggle (assumed default, no user objection).
---
<!-- COMMENTS:END -->
