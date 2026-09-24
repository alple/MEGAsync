---
id: MEGA-3.3
title: 'Stage 3: real providers — local walk + remote node-cache enumeration'
status: To Do
assignee: []
created_date: '2026-09-24 11:23'
updated_date: '2026-09-24 11:23'
labels:
  - sync-preview
milestone: Sync pre-commit review
dependencies:
  - MEGA-3.1
parent_task_id: MEGA-3
type: feature
ordinal: 19000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Implement the real providers behind the Stage-1 interfaces: local-side recursive filesystem walk (cancellable, QtConcurrent pattern like BackupCandidatesFolderSizeRequester; CRC via existing getCRC path with size short-circuit; error rows for unreadable/broken-symlink/zero-byte items) and remote-side enumeration via MegaApi::getChildren per folder over the node cache (ORDER_NONE fast path, batch overload, paged listChildNodesLexicographically for huge folders — verified 2026-09-24, see MEGA-3 decision records). Strictly read-only: no writes, no engine interaction. The demo keeps working by swapping fake↔real providers in configuration.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Local-side provider walks a real local directory tree asynchronously and cancellably, with CRC on demand (size short-circuit first), reporting unreadable files, broken symlinks, and zero-byte files as error rows
- [ ] #2 Remote-side provider enumerates the remote tree via getChildren per folder over the node cache (ORDER_NONE fast path; paged variant for huge folders), read-only
- [ ] #3 Both providers implement the Stage-1 interfaces; the classification/planning core is unchanged; switching between fake and real providers is a configuration change
- [ ] #4 Row-count and depth limits enforced with load-more continuation
- [ ] #5 Ignore-excluded items are listed with an advisory note (no matcher implemented in this stage)
<!-- AC:END -->
