---
id: MEGA-2.4
title: 'Stage 4: enforcement engine — apply approved plan, re-verify'
status: To Do
assignee: []
created_date: '2026-09-24 11:29'
updated_date: '2026-09-24 11:29'
labels:
  - sync-preview
milestone: Sync pre-commit review
dependencies:
  - MEGA-2.1
  - MEGA-2.3
parent_task_id: MEGA-2
type: feature
ordinal: 27000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Execute the approved operation plan using only verified existing primitives (MEGA-2 safety record): uploads via existing MegaUploader/startUpload; where the plan replaces a differing remote file, compose upload + moveNodeToRubbish(old node) as one logical recoverable operation (verified: COLLISION_RESOLUTION_* is download-only, uploads have no overwrite flag); downloads via MegaDownloader (collision flags apply to local placement); remote removals via moveNodeToRubbish; local removals to OS trash/backup — never hard unlink. Includes the re-verify pass (re-scan delta, re-flag changed rows) and per-row failure reporting without aborting the whole plan.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Executing an approved plan performs only verified operations: uploads, downloads (COLLISION_RESOLUTION_* where applicable — download-only), moveNodeToRubbish for remote removals, trash/backup move for local removals
- [ ] #2 "Make remote like local" on a differing file is composed as upload + moveNodeToRubbish(old node) — one logical operation, recoverable, no upload overwrite flag invented
- [ ] #3 Deletions are recoverable and surfaced in progress reporting; nothing hard-unlinks
- [ ] #4 Re-verify pass re-scans both sides after applying; rows whose classification changed are re-flagged and block commit until re-approved
- [ ] #5 Failures of individual operations are reported per row without aborting the whole plan, with a final summary
- [ ] #6 No SDK source files are modified
<!-- AC:END -->
