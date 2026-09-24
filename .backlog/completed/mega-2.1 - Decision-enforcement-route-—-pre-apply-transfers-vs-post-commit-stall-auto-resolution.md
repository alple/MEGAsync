---
id: MEGA-2.1
title: >-
  Decision: enforcement route — pre-apply transfers vs post-commit stall
  auto-resolution
status: Done
assignee: []
created_date: '2026-09-24 10:31'
updated_date: '2026-09-24 11:05'
labels:
  - 'wayfinder:grilling'
milestone: Sync pre-commit review
dependencies: []
parent_task_id: MEGA-2
priority: high
type: task
ordinal: 3000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Question

Which mechanism enforces the pre-commit review's local-wins/remote-wins choices for both-sides-differing items?

Route A — Pre-apply as plain transfers: before calling SyncController::addSync, resolve each choice manually (remote-wins: download the remote file over the local one; local-wins: upload the local file over the remote node — MEGA automatically keeps the overwritten remote file as a previous version). Then create the sync: the engine's first scan sees identical content and produces zero stalls by construction. Matches the user's framing ("it can work on just transfers for now... once all conflicts are resolved, I will create a sync"). Risks to weigh: staleness between apply and creation, collisions if the same remote node is written twice, loser-copy handling outside any sync context.

Route B — Post-commit stall auto-resolution: create the sync immediately; the engine stalls both-sides-differing files (SyncWaitReason::LocalAndRemotePreviouslyUnsyncedDiffer_userMustChoose — no transfer happens for them until resolved); apply the user's pre-made choices programmatically through the existing stalled-issues model (LocalOrRemoteUserMustChooseStalledIssue::chooseLocalSide/chooseRemoteSide). Pros: engine-observed conflicts, reuses prod resolution code verbatim, sync-aware debris handling free. Cons: non-conflicting transfers (local-only uploads, remote-only downloads) start at the moment of commit; the review dialog must survive until stalls appear; "commit" and "start transferring" become the same act.

Decide on: data safety, whether zero-transfer-before-commit is a hard requirement, minimal fork surface, restart/mid-review behavior. Both routes require zero SDK changes (established by research 2026-09-24, see parent map Notes).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A resolution comment records the chosen enforcement route with rationale, and the rejected option's reasons
- [ ] #2 The decision accounts for: loser-copy safety, whether any transfer may happen before the user's commit moment, app-restart during review, and fork-policy surface (both routes are app-side only)
- [ ] #3 Downstream tickets (loser's-copy, commit semantics, safety audit) can each be decided without revisiting this question
<!-- AC:END -->

## Comments

<!-- COMMENTS:BEGIN -->
created: 2026-09-24 11:05
---
Resolution (2026-09-24, user-defined): **pre-apply plan execution**. The review defines a directional plan per file/directory — make-remote-like-local (L→R), make-local-like-remote (R→L), or best-effort (both-way merge). Conflicts (same name both sides, including same-content-different-name) are flagged and require explicit approval; commit is blocked until every row is approved. The plan is executed as ordinary transfers + recoverable deletions BEFORE sync creation, so the engine's first scan sees identical content and produces zero stalls by construction. Post-commit stall auto-resolution rejected (would start bulk transfers at commit; dialog-survival fragility).
---
<!-- COMMENTS:END -->
