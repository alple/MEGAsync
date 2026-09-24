---
id: MEGA-3
title: 'Sync pre-commit review: diff, conflict resolution, then create syncs'
status: To Do
assignee: []
created_date: '2026-09-24 11:23'
labels:
  - sync-preview
milestone: Sync pre-commit review
dependencies: []
type: epic
ordinal: 16000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Pre-commit review for creating sync pairs in this MEGAsync fork. Today, creating a sync is blind: no way to see what the first sync will transfer, and both-sides-differing files only surface as stalls after the sync already exists. This effort delivers a review dialog: pick local↔remote "pre-sync pair(s)", see all differences, decide per file/directory what happens (recommended action marked, always overridable), approve everything, then create the sync via the standard create-sync dialog pre-filled — the engine's first scan then sees identical content and produces zero stalls by construction.

## User model (locked decisions)

- Dual-pane per pair: local on the left, remote on the right.
- Every row (file or directory) carries one of three actions: **L→R** (make remote like local: upload/overwrite remote; remote-only items are REMOVED from remote), **R→L** (make local like remote: download/overwrite local; local-only items are REMOVED locally), **best-effort** (both-way merge: missing side gets the file; same-name-differ = conflict). A recommended action is marked; the user overrides anything.
- Conflicts — same name on both sides with different content, same content but different names, case-insensitive name collisions, file-vs-folder type mismatches — are flagged and **require explicit approval**. Commit is blocked until every row is approved ("don't allow to proceed unless everything is approved by me").
- Directory-level actions cascade to all contained rows and show a **consequences popup** (which files removed, which changed).
- Deletions are **recoverable**: remote → MEGA Rubbish; local → OS trash/backup folder. Never hard unlink.
- Identical rows hidden by default with an "in sync" toggle. Binary-first: metadata diff only (size, modified date, newer-side marker) — no textual diffs.
- Multi-pair: the dialog queues several candidate pairs before committing any.
- **Full persistence**: queued pairs AND per-row decisions survive closing the window and app restarts. Storage: fork-owned JSON file in the app config directory (new file, zero upstream conflict). On reopen the queue is restored and re-scanned; rows whose classification changed are re-flagged; pairs drop off after their sync is created.
- **Commit flow**: review (everything approved) → open the standard create-sync dialog **pre-filled** with the pair's local/remote paths where supported → on user confirm: apply the approved plan → re-verify → sync created active. Cancel at the create dialog applies nothing (plan execution happens only on final confirm).

## Decision records

- **Enforcement**: pre-apply plan execution — resolutions become ordinary transfers + recoverable deletions BEFORE sync creation, so the engine's first scan sees identical content (zero stalls by construction). Post-commit stall auto-resolution rejected (bulk transfers start at commit; dialog-survival fragility).
- **Data source**: app-side scan + classification, zero SDK changes (no dry-run exists; `Sync::isScanOnly` is periodic-rescan, not a dry-run). Remote enumeration: `getChildren` per folder on the node cache (ORDER_NONE fast path; batch overload; paged variant for huge trees). .megaignore: parser exists sync-independently, no app-side matcher (matching lives in sync-internal SDK `FilterChain`) — v1 lists ignore-excluded items with an advisory note; FilterChain reuse is a later option. Engine remains the authority; preview is advisory-only.
- **UI placement**: standalone widget dialog (StalledIssuesDialog module pattern) — smallest measured merge surface. Touch points ledger: new `sync_preview/` module (zero upstream conflict) + ~10-15-line hook in MegaApplication.cpp beside showStalledIssuesDialog + small Review button in the Add-Sync wizard + tray/syncs-menu item; plus a small pre-fill/plumbing edit in the wizard's SyncsData/SyncsComponent for handing a reviewed pair into the standard create flow (fallback if infeasible: the review dialog hosts the final confirmation itself and calls SyncController::addSync directly). QML-wizard-page and TransferManager-tab rejected on churn cost.
- **Row model**: three actions per file and per directory (L→R / R→L / best-effort); recommended action marked, everything overridable; conflicts flagged and explicitly approved (includes same-content-different-name); directory actions show consequences popup (files removed, changed); identical rows hidden by default with toggle.
- **Deletion policy**: recoverable — remote → MEGA Rubbish (`moveNodeToRubbish`), local → OS trash/backup folder; never hard unlink.
- **Commit semantics**: approval gate → pre-filled standard create-sync dialog → on confirm apply plan → re-verify → create active. Unapproved rows block commit. Per-pair or batch commit.
- **Safety checklist**: blocker rows (case-insensitive name collisions, file-vs-folder type mismatches — cannot be resolved by a transfer; require rename/exclusion); staleness re-scan with re-flagging on reopen and at commit; upload-replace = upload + moveNodeToRubbish(old) as one logical recoverable operation (verified: COLLISION_RESOLUTION_* is download-only, uploads have no overwrite flag); upload-size estimate pre-commit; row caps with load-more; full persistence of queue + decisions with re-verify on load; ignore-excluded items listed with advisory note.

## Architecture

- Zero SDK changes; purely additive app-side module `sync_preview/`. Engine remains the authority; the preview is advisory-only and its enforcement uses existing app APIs only.
- **Provider seam (fake-data-first)**: comparison + planning core sits behind provider interfaces (local-side, remote-side). A fake provider generates trees with edge cases (nested dirs, same-name-diff-content, same-content-diff-name, type mismatches, case collisions, empty dirs, deep trees) so the GUI is developed and demoed on generated data before real integration; real providers plug into the same seam later and the demo must work unchanged with real prod data.
- Classification rows: local-only / remote-only / identical (content-equal by CRC when size matches) / both-differ / conflict / blocker. Content equality: size short-circuit, then CRC (`getCRC` local vs `getCRCFromFingerprint` remote).
- Enforcement primitives (verified 2026-09-24): uploads via existing `MegaUploader`/`startUpload` (no upload overwrite flag — compose upload + Rubbish-old for replacement); downloads via `MegaDownloader` (collision flags apply to local placement); remote deletions via `moveNodeToRubbish`; remote enumeration via `getChildren` (ORDER_NONE fast path; paged variant for huge trees).
- Persistence: fork-owned JSON (pairs, per-row decisions, queue state) in the app config directory; versioned schema; corruption falls back to an empty queue.

## Implementation stages

1. **Core: providers seam + fake-data engine** — provider interfaces, fake provider with generated edge cases, classification + consequences planner ({upload, download, deleteRemote, deleteLocal, none}), persistence serialization, unit tests. No UI, no real API.
2. **Review dialog on fake data** — dual-pane list, per-row/per-directory actions, conflict flags, approval gating, consequences popup, multi-pair queue, persisted-queue restore; runs entirely on the fake provider.
3. **Real providers** — local walk (cancellable, CRC) + remote enumeration (getChildren), read-only, replacing fake in the same seam.
4. **Enforcement engine** — execute the approved plan with verified primitives (upload+Rubbish-old composition, downloads, recoverable deletions), re-verify pass.
5. **Commit orchestration + entry points** — approval gate, batch commit, hand-off into the pre-filled standard create-sync dialog (fallback: in-dialog confirm calling SyncController::addSync), wizard button + tray entry + MegaApplication hook.
6. **Safety checklist + E2E verification** — safety checklist as hard requirements; fresh-account probe proving the preview matches engine verdicts.

Dependency spine: 1 → 2; 1 → 3 → 4; (2,4) → 5; (3,4) → 6.

## Out of scope

Gating live syncs; SDK modifications; persistent snapshot caching; textual diffs; move/rename detection in preview; per-row ignore-rule authoring (advisory note instead).

(Rebuilt from MEGA-2 at user request so child numbering starts at .1; MEGA-2 archived.)
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The epic body is the implementation spec including all decision records inlined: goal, locked user model, architecture with provider seam, stage list with dependency spine, out-of-scope list — no fog remains
- [ ] #2 Implementation stages exist as children numbered from .1 with the dependency spine wired (1→2, 1→3→4, (2,4)→5, (3,4)→6)
- [ ] #3 Every stage's operation vocabulary references only verified real-API actions; no SDK source modifications are prescribed anywhere
- [ ] #4 No real account names, personal paths, or absolute machine paths appear in any ticket text
<!-- AC:END -->
