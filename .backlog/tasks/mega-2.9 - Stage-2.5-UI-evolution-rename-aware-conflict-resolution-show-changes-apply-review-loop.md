---
id: MEGA-2.9
title: >-
  Stage 2.5 UI evolution: rename-aware conflict resolution + show-changes/apply
  review loop
status: Backlog
assignee: []
created_date: '2026-09-25 16:09'
labels:
  - sync-preview
milestone: Sync pre-commit review
dependencies:
  - MEGA-2.8
parent_task_id: MEGA-2
type: enhancement
ordinal: 32000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Spun out of MEGA-2.8 Testing feedback (2026-09-25): the review UI needs rename-aware resolution and an applied-changes loop — UI design now, engine semantics on fake data (Stage 2), real execution stays Stage 5 (MEGA-2.5).

**1. Rename-aware transfers on conflict/blocker rows (decided direction):**
- Choosing → / ← on a conflict or blocker row must mean "transfer with automatic rename" (decision-level); the action panel presents rename/exclusion as the presented solution for rows "not resolvable by a transfer". Blocker rows keep transfer buttons visible-but-disabled until this ticket makes them decidable.
- Reconcile approval gating with "do nothing": a do-nothing row must NOT count as awaiting approval (the UI already hides the Approve toggle for effective=None; the engine's awaitingApprovalCount/allApproved still counts it — fix here, not in the UI).

**2. Rename-swap edge case (fish for more like it):**
- Scenario: local foo.txt@hash_1 | remote foo.txt@hash_2 + remote bar.txt@hash_1 — a naive L→R overwrites content the remote already keeps under bar.txt. Resolution must offer the rename-aware swap.
- Classifier work: extend twin detection to content matches for PAIRED rows (today Pass C only matches local-only placeholders against remote leftovers, so bar.txt reads as plain RemoteOnly with no twin hint).
- The `renameSwap` fake scenario is already added (MEGA-2.8) and pickable in the pair picker; use it as the acceptance fixture. Fish for sibling cases (double rename, rename+edit, rename across folders) and pin them as scenarios/tests.

**3. Show changes / Apply review loop:**
- A "Show changes" button (pair list and/or detail window) listing all scheduled (approved) changes: row, action, from → to path (renames included).
- An "Apply" step that executes the scheduled changes on the FAKE data and re-resolves the two panes (applied rows vanish or turn Same; parents re-read Identical), so the reviewer goes back and forth: apply → inspect → adjust → apply again. No real API calls — still Stage 2 fake data; the real execution entry point remains Stage 5 (MEGA-2.5).

**4. Fake-data depth:** continue densifying the canned scenarios (kitchen sink already carries a 4-level nested branch from MEGA-2.8).

Scope guard: no real sync/transfer behavior; model (classifier/planner) changes are fork-side sync_preview only; nothing upstream-touched.
<!-- SECTION:DESCRIPTION:END -->
