---
id: MEGA-2.6
title: 'Stage 6: safety checklist + E2E verification against the engine'
status: To Do
assignee: []
created_date: '2026-09-24 11:29'
updated_date: '2026-09-24 11:29'
labels:
  - sync-preview
  - testing
milestone: Sync pre-commit review
dependencies:
  - MEGA-2.3
  - MEGA-2.4
parent_task_id: MEGA-2
type: feature
ordinal: 29000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Implement the MEGA-2 safety checklist as hard requirements and verify the preview against the real engine: staged end-to-end probe on a test account — build candidate pairs with known content, run the review+commit flow, and compare what the preview promised (operations per row) against what the engine actually did (transfers via getTransfers, stalls via stall list) for fresh-pair creation with both-sides content. Document any mismatch; the preview is advisory-only so the engine is the tiebreaker, and mismatches feed fixes under this stage.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Every item of the MEGA-2 safety checklist is implemented or explicitly deferred with a note: blocker rows (case collisions, type mismatches), staleness re-scan on reopen and at commit, quota estimate, row caps, persistence with re-verify on load
- [ ] #2 An E2E probe on a test account compares preview verdicts against actual engine behavior (transfers performed, stalls raised) for staged scenarios and documents matches/mismatches
- [ ] #3 Case-collision and type-mismatch blocker rows prevent commit until renamed/excluded by the user
- [ ] #4 Preview/engine mismatch findings are recorded with evidence; fixes land as follow-up edits under this stage
<!-- AC:END -->
