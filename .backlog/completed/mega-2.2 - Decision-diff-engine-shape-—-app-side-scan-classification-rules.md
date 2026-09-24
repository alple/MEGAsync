---
id: MEGA-2.2
title: 'Decision: diff engine shape — app-side scan + classification rules'
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
ordinal: 4000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Question

How does the preview compute the would-transfer list for a candidate sync pair, app-side and with zero engine involvement?

Established facts (research 2026-09-24): the engine's first-scan classification is deterministic — local-only → upload, remote-only → download, identical fingerprint → matched, both-sides-differ → user-choice stall. The preview must reproduce that classification advisory-only. Existing primitives: the CheckDuplicatedNodes name-hash pattern (one getChildren + case-insensitive name/type matching, transfers/gui/DuplicatedNodeDialogs/DuplicatedNodeInfo.cpp), LocalFileFolderAttributes vs RemoteFileFolderAttributes CRC requests for true content-equality, FileFolderAttributes for size/mtime (2s TTL cache), SDK node cache via getChildren.

Decide: recursive walk strategy and depth limits for large trees; classification rules (size/mtime/CRC thresholds — when is a same-name file "identical" without CRCing everything); async/cancellable worker design (QtConcurrent pattern exists in BackupCandidatesFolderSizeRequester); memory/row-count bounds and truncation policy; what the preview deliberately does NOT attempt (move/rename detection — the engine does move detection the preview cannot; deletion is out of scope since nothing is deleted at creation).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A resolution comment records the scan/compare design: walk strategy, classification rules per row type, chosen primitives, and depth/performance/cancellation limits
- [ ] #2 The design honors the advisory-only constraint: no engine side effects, no writes, engine remains authoritative at creation time
- [ ] #3 The design states how content-equality is decided (CRC comparison primitive) and what happens for folders, empty folders, and huge trees
- [ ] #4 A decision records whether .megaignore/exclusion awareness belongs in the preview or is deferred with a stated consequence
<!-- AC:END -->

## Comments

<!-- COMMENTS:BEGIN -->
created: 2026-09-24 11:05
---
Resolution (2026-09-24): **app-side scan + classification, zero SDK changes** (no dry-run exists; isScanOnly is periodic-rescan, not a dry-run; engine cannot be consulted pre-creation). Verified implementation facts: remote enumeration = `MegaApi::getChildren` per folder over the node cache (ORDER_NONE fast path for large folders, batch overload for multiple parents, `listChildNodesLexicographically` paged variant for huge trees; cache must be fully loaded). .megaignore: `MegaIgnoreManager` parses rules from any folder without a sync but provides NO matcher — matching lives only in the SDK's `FilterChain` (sync-internal, RemotePathPair-based). Deferred to implementation: reuse FilterChain call-only (no SDK edit) if practical, else v1 lists ignore-excluded items with an advisory note. Engine remains the authority at creation time; preview is advisory-only.
---
<!-- COMMENTS:END -->
