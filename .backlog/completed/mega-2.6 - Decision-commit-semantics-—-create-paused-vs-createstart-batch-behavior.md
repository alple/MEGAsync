---
id: MEGA-2.6
title: 'Decision: commit semantics — create paused vs create+start, batch behavior'
status: Done
assignee: []
created_date: '2026-09-24 10:32'
updated_date: '2026-09-24 11:05'
labels:
  - 'wayfinder:grilling'
milestone: Sync pre-commit review
dependencies:
  - MEGA-2.1
parent_task_id: MEGA-2
type: task
ordinal: 8000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Question

After the review completes, what does "commit" do — across all reviewed sync pairs?

Constraints from the map: the user's stated flow is "once all of the conflicts are resolved, I will create a sync". If the enforcement route is pre-apply (Route A), create+start is safe by construction because conflicts are resolved before creation. If post-commit (Route B), commit and start are the same act and unresolved rows would need a policy (stall later, or block commit).

Decide: create immediately vs create paused (existing pause machinery — verify paused syncs truly do not scan/transfer); handling of rows the user left unresolved; batch semantics when several pairs are queued (all-or-nothing vs per-pair commit); post-commit behavior of the review UI.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A resolution comment records the commit action and its rationale, including behavior for unresolved leftover rows and for multi-pair batches
- [ ] #2 The decision verifies whether paused syncs perform no transfers (existing pause machinery) if the paused option is chosen
- [ ] #3 The decision states what the review UI does after commit (closes, reports progress, hands off)
<!-- AC:END -->

## Comments

<!-- COMMENTS:BEGIN -->
created: 2026-09-24 11:05
---
Resolution (2026-09-24): commit = apply the approved plan → **re-verify** (re-scan delta; rows whose classification changed get re-flagged for re-approval) → **create the sync(s) immediately active**. Rows not approved block commit with a warning — the user said: don't allow to proceed unless everything is approved. Batch semantics: per-pair commit or commit-all. Paused creation rejected (conflicts are resolved pre-creation, so paused adds no safety).
---
<!-- COMMENTS:END -->
