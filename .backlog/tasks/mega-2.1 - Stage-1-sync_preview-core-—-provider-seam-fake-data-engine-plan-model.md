---
id: MEGA-2.1
title: 'Stage 1: sync_preview core — provider seam, fake-data engine, plan model'
status: To Do
assignee: []
created_date: '2026-09-24 11:29'
labels:
  - sync-preview
milestone: Sync pre-commit review
dependencies: []
parent_task_id: MEGA-2
priority: high
type: feature
ordinal: 24000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Build the sync-preview core with no UI and no real API, per the MEGA-2 spec: provider interfaces (local-side, remote-side), a fake provider generating trees with edge cases for GUI development and demoing before real integration, the classification engine (local-only / remote-only / identical / both-differ / conflict / blocker, CRC-based content equality with size short-circuit), the consequences planner that turns a chosen action (L→R / R→L / best-effort, per file or directory) into an operation list ({upload, download, deleteRemote→Rubbish, deleteLocal→trash/backup, none}) with directory cascades, and JSON serialization of the pair queue + per-row decisions (versioned schema, corrupt file falls back to empty queue). The operation vocabulary may only use actions verified to exist in the real API (see MEGA-2 decision records). Unit-tested.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Provider interfaces for local-side and remote-side trees exist, with the comparison/planning core depending only on the interfaces
- [ ] #2 Fake provider generates trees covering: nested directories, empty dirs, same-name-different-content, same-content-different-name, file-vs-folder type mismatches, case-insensitive name collisions, deep trees
- [ ] #3 Classification produces rows: local-only, remote-only, identical, both-differ, conflict, blocker
- [ ] #4 Consequences planner maps each action (L→R, R→L, best-effort) per row/directory to operations {upload, download, deleteRemote, deleteLocal, none} including directory cascades
- [ ] #5 Operation vocabulary uses only verified real-API actions (upload, download, moveNodeToRubbish, trash/backup delete, none)
- [ ] #6 Pair queue state (pairs, per-row decisions) can be serialized to and restored from a versioned JSON schema; corrupted files fall back to an empty queue
- [ ] #7 Unit tests cover classification and planning on generated edge cases
<!-- AC:END -->
