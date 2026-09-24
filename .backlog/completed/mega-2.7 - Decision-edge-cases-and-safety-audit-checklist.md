---
id: MEGA-2.7
title: 'Decision: edge cases and safety audit checklist'
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
ordinal: 9000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Question

Which edge cases and hazards must the preview and resolution flow handle, and how?

Enumerate and decide (informed by engine stall taxonomy): case-insensitive name collisions (engine stall NamesWouldClashWhenSynced — two local files differing only by case, or local/remote case variants); file-vs-folder type mismatches (FolderMatchedAgainstFile); files changed between scan, review, and commit (staleness policy: auto re-scan, stale-row marking); unreadable/permission-denied local files; broken symlinks and zero-byte files; storage-quota exhaustion between preview and commit (quota exists at creation but content volume is not pre-checked); .megaignore exclusions the engine will honor but a naive scan lists anyway; deep trees and row-count truncation; app restart or dialog close mid-review (state persistence vs discard). Output is a safety checklist the implementation tickets must satisfy. Depends on the enforcement-route decision.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A resolution comment records the safety checklist covering each enumerated edge case with decided handling or explicit deferral
- [ ] #2 Case-insensitive name collisions and file-vs-folder type mismatches have decided handling for the chosen enforcement route
- [ ] #3 Staleness between scan, review, and commit has a decided policy (re-scan trigger, or mark rows stale)
- [ ] #4 The checklist is written so implementation tickets can reference it as a hard requirement list
<!-- AC:END -->

## Comments

<!-- COMMENTS:BEGIN -->
created: 2026-09-24 11:05
---
Resolution (2026-09-24): checklist updated for the directional model. (1) Recoverable deletions both sides (remote→Rubbish, local→trash/backup). (2) Blocker rows that a transfer cannot resolve: case-insensitive name collisions, file-vs-folder type mismatches — flagged, require rename/exclusion before commit is enabled. (3) Staleness: re-scan at commit click; rows whose classification changed are re-flagged for approval. (4) Upload-replace mechanics: verified that COLLISION_RESOLUTION_* is download-only and uploads have NO overwrite flag — 'make remote like local' on a differing file = startUpload (new content, may land as duplicate-name node) + moveNodeToRubbish(old node); composed as one logical operation, recoverable. (5) Upload-size estimate shown pre-commit (quota sanity). (6) Row-count cap with load-more for huge trees. (7) Review state lives in the dialog; app restart discards it (re-adding a pair is cheap); no persistence in v1. (8) Ignore-excluded items listed with advisory note (ignore matching deferred — see MEGA-2.2). Implementation stages must satisfy this list.
---
<!-- COMMENTS:END -->
