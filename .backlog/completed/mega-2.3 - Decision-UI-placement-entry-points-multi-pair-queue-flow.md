---
id: MEGA-2.3
title: 'Decision: UI placement, entry points, multi-pair queue flow'
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
ordinal: 5000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Question

Where does the review UI live, how is it opened, and how does the user queue several candidate sync pairs before committing any?

Merge-surface research (2026-09-24) ranked the options: (a) standalone widget dialog modeled on StalledIssuesDialog — smallest surface: new module + ~10-15-line hook in MegaApplication.cpp next to showStalledIssuesDialog() + one entry point; (b) new page inside the QML Add-Sync wizard — moderate (~6-8 small files in a churn-dense QML dir; upstream deleted a similar candidates-confirmation step SNC-5743); (c) TransferManager tab — largest (1923-line .ui edit, TM_TAB enum arithmetic ripple, transfer-filter domain mismatch). Recommended hybrid: standalone dialog + one small "Preview" button edit in the wizard (AddSyncPage.qml/SelectiveSyncPage.qml) + optional tray entry via SyncsMenu.

Decide: final placement, entry points, how the user adds/removes/queues several candidate pairs before committing any (today the wizard commits one pair immediately), and whether the wizard's final button becomes "Review" by default or stays "Create" with review as an alternative action.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A resolution comment records: placement, entry point(s), and the pair-queueing flow with rationale and rejected alternatives
- [ ] #2 The decision includes the measured merge-surface cost of the chosen option (existing files edited, churn rank) versus the alternatives
- [ ] #3 The flow supports reviewing several candidate pairs before committing any of them, consistent with the 'filterable single list, default grouped by sync' decision on the map
<!-- AC:END -->

## Comments

<!-- COMMENTS:BEGIN -->
created: 2026-09-24 11:05
---
Resolution (2026-09-24): **standalone widget dialog** modeled on StalledIssuesDialog — smallest measured merge surface (new module `sync_preview/`, ~10-15-line hook in MegaApplication.cpp next to showStalledIssuesDialog(), one small Review button in the Add-Sync wizard, one tray/syncs-menu entry). User confirmed the dialog flow: open dialog → create pre-sync pair(s) → differences listed → decide per row/directory. Multi-pair queue lives inside the dialog; QML-wizard-page and TransferManager-tab options rejected on measured churn cost.
---
<!-- COMMENTS:END -->
