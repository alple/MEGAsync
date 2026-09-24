---
id: MEGA-2.1
title: 'Stage 1: sync_preview core — provider seam, fake-data engine, plan model'
status: Testing
assignee:
  - kilo
created_date: '2026-09-24 11:29'
updated_date: '2026-09-24 13:14'
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
- [x] #1 Provider interfaces for local-side and remote-side trees exist, with the comparison/planning core depending only on the interfaces
- [x] #2 Fake provider generates trees covering: nested directories, empty dirs, same-name-different-content, same-content-different-name, file-vs-folder type mismatches, case-insensitive name collisions, deep trees
- [x] #3 Classification produces rows: local-only, remote-only, identical, both-differ, conflict, blocker
- [x] #4 Consequences planner maps each action (L→R, R→L, best-effort) per row/directory to operations {upload, download, deleteRemote, deleteLocal, none} including directory cascades
- [x] #5 Operation vocabulary uses only verified real-API actions (upload, download, moveNodeToRubbish, trash/backup delete, none)
- [x] #6 Pair queue state (pairs, per-row decisions) can be serialized to and restored from a versioned JSON schema; corrupted input falls back to an empty queue — Stage 1 delivers the in-memory codec (user decision 2026-09-24); file I/O + config-dir wiring deferred to Stage 2/5
- [x] #7 Unit tests cover classification and planning on generated edge cases
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Stage 1 plan (user-approved 2026-09-24):

**New module** `src/MEGASync/sync_preview/` (model/ only; gui/ arrives in Stage 2):
- `syncpreview.cmake` — module file, included by app + UnitTests targets
- `model/SyncPreviewDefs.h` — enums: EntryType, RowKind, Action, OperationType, BlockerReason
- `model/SyncPreviewTree.h/.cpp` — Entry + TreeSnapshot (relative-path entries: name/type/size/mtime/contentHash) + path helpers
- `model/SyncPreviewProviders.h` — abstract LocalSideProvider / RemoteSideProvider (snapshot()); core depends only on these
- `model/FakeSyncPreviewProvider.h/.cpp` — fake provider + edge-case tree builder (nested dirs, empty dirs, same-name-diff-content, same-content-diff-name, file-vs-folder mismatch, case-insensitive collisions, deep trees)
- `model/SyncPreviewClassifier.h/.cpp` — rows LocalOnly/RemoteOnly/Identical/BothDiffer/Conflict/Blocker; size short-circuit → CRC equality
- `model/SyncPreviewPlanner.h/.cpp` — decisions + cascades → ops {Upload, Download, UploadReplace, DownloadReplace, DeleteRemoteToRubbish, DeleteLocalToTrash, None} + consequences (removed/changed paths, duplicate warnings)
- `model/SyncPreviewQueue.h` + `SyncPreviewQueueStore.h/.cpp` — versioned JSON serialize/restore of pairs + per-row decisions; garbage input → empty queue

**Wiring**: `include(sync_preview/syncpreview.cmake)` in `src/MEGASync/CMakeLists.txt` + `src/MEGAAutoTests/UnitTests/CMakeLists.txt`; tests under `src/MEGAAutoTests/UnitTests/sync_preview/`; run via `just test`.

**Locked decisions (user, 2026-09-24)**:
- Same-content-different-name = separate per-side rows (no merged pair row), kind Conflict, advisory twin flag, independent per-row decisions
- Replace = single logical ops UploadReplace/DownloadReplace; Stage 4 expands to verified primitives (upload + moveNodeToRubbish(old); download with collision handling)
- QueueStore Stage 1 = in-memory serialize/restore only; real file I/O + config-dir wiring deferred to Stage 2/5 (AC #6 "corrupted files" satisfied as corrupt-JSON restore → empty queue)
- Cascade: own explicit decision > nearest ancestor dir explicit decision > recommended action
- Recommendations: local-only→L→R; remote-only→R→L; identical→none; both-differ→newer mtime (tie→L→R); conflict/blocker→none (explicit approval required)
- No VER_FORK_SUFFIX bump in Stage 1 (test binaries only; bump applies when a production binary ships)
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implementation log: two classifier bugs were caught by the suite and fixed — remote-leftover rows were appended without their relativePath, and twin-consumed remote entries were skipped instead of emitted as the remote side of the twin pair.

The app target compiles with -Werror=dangling-reference (the UnitTests target does not): fixed by taking QHash::value() by value instead of binding a reference through constFind(...).value().

CONTEXT.md created at the repo root with the sync-preview glossary (single-context layout per docs/agents/domain.md).

Stage 4 follow-up discovered while planning ops: subtree Upload/Download ops on directories need a remote-folder-creation primitive (MegaApi::createFolder app-side, no SDK modification) which is NOT in the epic's verified-primitives list — must be verified in the MEGA-2.4 ticket before enforcement.

No VER_FORK_SUFFIX bump: only the UnitTests test binary and the dev app build were produced; bump applies when a production binary ships.

Added a human-readable demo dump so the headless Stage 1 core can be inspected without the Stage 2 dialog: `just demo-syncpreview` prints the kitchen-sink classification, the recommended plan, and a plan with sample decisions (cascades, overrides, warnings, consequences). It is a Catch2 test (SyncPreviewDemoTests.cpp) run with -s success output; added to the justfile as demo-syncpreview.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Stage 1 of the sync pre-commit review delivered: the sync_preview core — provider seam, fake-data engine, classification, consequences planner, queue serialization — with no UI, no real API, and zero SDK changes.

**New module `src/MEGASync/sync_preview/`** (model/ only; gui/ arrives in Stage 2):
- `SyncPreviewProviders.h` — LocalSideProvider / RemoteSideProvider interfaces; the classifier and planner depend only on these (AC #1).
- `SyncPreviewTree.h/.cpp` — Entry + materialized Tree snapshot (relative paths, type/size/mtime/contentHash); `sameContent` = size short-circuit then CRC (missing CRC counts as different, conservatively).
- `FakeSyncPreviewProvider.h/.cpp` — FakeTreeBuilder + FakeSideProvider (implements both sides) + canned scenarios: edgeCaseKitchenSink (every AC #2 edge case in one pair), deepTree, emptySides (AC #2).
- `SyncPreviewClassifier.h/.cpp` — exact-path pairing → LocalOnly/RemoteOnly/Identical/BothDiffer; case-insensitive collision pass → merged Blocker rows (collision or type mismatch); same-content-different-name pass → separate per-side Conflict rows with advisory twin flag (locked decision); underBlockedPath advisory for type-mismatch descendants; locked recommendation policy (AC #3).
- `SyncPreviewPlanner.h/.cpp` — effective decisions (own > ancestor > recommended); op vocabulary {Upload, Download, UploadReplace, DownloadReplace, DeleteRemoteToRubbish, DeleteLocalToTrash, None} — replace ops are single logical operations the Stage 4 engine expands into verified primitives (upload + moveNodeToRubbish(old); download with local collision handling); uniform single-sided dirs emit subtree ops, mixed dirs emit node ops + per-row ops; subtree deletions dedupe matching explicit deletions and warn on keepers; consequences (created/changed/removed per side) aggregate deepest-first for the directory popup; duplicate warnings on conflict transfers (AC #4, #5).
- `SyncPreviewQueue.h` + `SyncPreviewQueueStore.h/.cpp` — versioned JSON (schemaVersion 1) of pairs + per-row decisions incl. approvals; all-or-nothing restore (unparseable / wrong-version / structurally invalid → empty queue). In-memory codec per locked decision; file I/O + config-dir wiring deferred to Stage 2/5 (AC #6 as amended).

**Wiring (additive)**: one `include()` line each in `src/MEGASync/CMakeLists.txt` and the UnitTests target; 32 new Catch2 test cases under `src/MEGAAutoTests/UnitTests/sync_preview/` (AC #7).

**Verification**: full suite green — 87 cases / 516 assertions via `just test` (known-broken upstream cases excluded per MEGA-1); MEGAsync app target builds green (compiles under QT_NO_CAST_FROM_ASCII and -Werror=dangling-reference). Two classifier bugs were caught by the tests and fixed during the loop.

**Risks/follow-ups**: Stage 4 must verify a remote-folder-creation primitive (MegaApi::createFolder app-side, no SDK modification) — subtree Upload/Download ops on directories need it and it is not in the epic's verified-primitives list; queue file I/O + config-dir path lands with Stage 2/5 wiring; no VER_FORK_SUFFIX bump (test/dev binaries only).
<!-- SECTION:FINAL_SUMMARY:END -->
