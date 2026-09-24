---
id: MEGA-2.12
title: 'Stage 5: commit orchestration + entry-point wiring'
status: To Do
assignee: []
created_date: '2026-09-24 11:06'
updated_date: '2026-09-24 11:23'
labels:
  - sync-preview
milestone: Sync pre-commit review
dependencies: []
parent_task_id: MEGA-2
type: feature
ordinal: 14000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Wire the review into the app: commit orchestration per the updated user model — approval gate (commit disabled until all rows approved), then hand the approved pair into the standard create-sync dialog pre-filled (small plumbing edit in SyncsData/SyncsComponent); the approved plan is applied (Stage 4) only on the user's final confirm in that dialog, then re-verified and the sync created active; canceling applies nothing. Fallback if pre-fill proves infeasible: in-dialog confirmation calling SyncController::addSync directly. Plus the three entry points from the MEGA-2 ledger: MegaApplication hook beside showStalledIssuesDialog, Review button in the Add-Sync wizard, tray/syncs-menu item. Merge surface stays at the ledgered minimum.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Commit is enabled only when every row of a pair is approved; unapproved rows block with a warning listing them
- [ ] #2 Commit hands the approved pair to the standard create-sync dialog pre-filled with local/remote paths (small plumbing edit in SyncsData/SyncsComponent); on the user's final confirm the plan is applied (Stage 4), re-verified, and the sync is created active — canceling the create dialog applies nothing
- [ ] #3 If pre-fill proves infeasible, the fallback is an in-dialog confirmation that calls SyncController::addSync directly after plan application — decision recorded with rationale either way
- [ ] #4 Per-pair and commit-all batch flows exist; the persisted queue drops pairs whose sync was created; post-commit the dialog reports progress and hands off without orphan state
- [ ] #5 Entry points wired: small Review button in the Add-Sync wizard, tray/syncs-menu item, ~10-15-line showSyncPreviewDialog hook in MegaApplication.cpp beside showStalledIssuesDialog
- [ ] #6 Upload-size estimate shown at commit for quota sanity
- [ ] #7 Existing upstream files edited only at the ledgered touch points (MegaApplication hook, wizard button, menu entry, SyncsData/SyncsComponent plumbing)
<!-- AC:END -->
