---
id: MEGA-2.2
title: 'Stage 2: review dialog UI on fake data'
status: Testing
assignee:
  - '@kilo'
created_date: '2026-09-24 11:29'
updated_date: '2026-09-24 14:51'
labels:
  - sync-preview
milestone: Sync pre-commit review
dependencies:
  - MEGA-2.1
parent_task_id: MEGA-2
type: feature
ordinal: 25000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Build the review dialog UI (standalone widget dialog, StalledIssuesDialog module pattern — all-new files, per MEGA-2 UI decision) running entirely on the Stage-1 fake provider: dual-pane rows (local left / remote right), per-row and per-directory action choice (L→R / R→L / best-effort) with recommended action marked and overridable, conflict/blocker flags with explicit approval, commit gated until all rows approved, directory-action consequences popup, identical-rows hidden by default with toggle, row-count cap with load-more, multi-pair queue (add/remove pairs, filterable list grouped by pair), and restore of the persisted pair queue on reopen. No real API integration in this stage.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Dual-pane per-pair view (local left, remote right) listing all classified rows with per-side size and modified date and a newer-side marker
- [ ] #2 Per-row and per-directory action selection among L→R / R→L / best-effort, with the recommended action pre-marked and everything overridable
- [ ] #3 Conflicts and blocker rows are visually flagged and require explicit approval; commit stays disabled until all rows are approved
- [ ] #4 Directory-level actions trigger a consequences popup listing which files are removed and which changed
- [ ] #5 Identical rows hidden by default with an 'in sync' toggle; huge lists capped with load-more
- [ ] #6 Multi-pair queue: several candidate pairs can be added, reviewed, and removed; list filterable, grouped by pair by default
- [ ] #7 Runs entirely on the fake provider; no real API calls
- [ ] #8 Closing and reopening the dialog restores the persisted pair queue and decisions; changed classifications are re-flagged; completed pairs are dropped
- [ ] #9 Dev-run console (`just run`) no longer floods with the repeated fs.cpp mount-database log lines (verified live: 1 console line in 10s vs 1092 before); the full stdout mirror stays available via the `--debug` runtime flag; log-file verbosity unchanged
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Stage 2 plan (drafted 2026-09-24, pending user approval):

**Architecture**: the dialog is provider-agnostic — it drives the Stage-1 seam (FakeSideProvider in this stage) through a headless PairController, so Stage 3/5 swap providers and pair sources without touching the dialog. All-new files in the sync_preview module; only additive wiring in existing files.

**New model files** (`src/MEGASync/sync_preview/model/`):
- `SyncPreviewQueueFileStore.h/.cpp` — file-backed persistence over the Stage-1 QueueStore codec: save via QSaveFile (atomic), load with missing-file → empty queue, corrupt → empty queue (QueueStore semantics). File path injected by the caller (demo now; app config dir via Preferences in Stage 5).
- `SyncPreviewReconciler.h/.cpp` — restore-time re-verify: re-scan each pair, drop decisions for vanished paths, re-flag rows whose classification changed (clear approval, record re-flagged set), drop pairs marked completed. Pure + unit-tested.

**Model extensions** (fork-added files, additive):
- `SyncPreviewQueue.h`: RowDecision gains persisted kind + requiresApproval snapshots (needed for "changed classifications are re-flagged"); Pair gains a `completed` flag (Stage 5 sets it on commit; reconcile drops completed pairs on load).
- `SyncPreviewQueueStore.h/.cpp`: SCHEMA_VERSION 2 with v1 migration (v1 = no snapshots → treated as changed → re-flagged on first restore).

**New gui files** (`src/MEGASync/sync_preview/gui/`, StalledIssuesDialog module pattern):
- `SyncPreviewPairController.h/.cpp` — headless orchestrator: queue state, per-pair classification (re-scan via providers), decisions/approvals, plan() via Stage-1 Planner, approval-gate computation, persistence on every mutation, reconcile-on-load. No widgets → unit-testable. Pair source injected as a factory (Stage 2: fake scenarios; Stage 5: real picker).
- `SyncPreviewDialog.h/.cpp` + `gui/ui/SyncPreviewDialog.ui` — main dialog: QTreeWidget grouped by pair (pairs = top-level nodes with Remove button, rows = children); text filter; "Group by pair" toggle; "Show in-sync items" toggle (identical rows hidden by default); per-pair Commit button gated on all flagged rows approved (Stage 2 click = placeholder notice; real commit flow is Stage 5).
- `SyncPreviewRowWidget.h/.cpp` — one review row: relative path, local size/mtime, remote size/mtime, newer-side marker, action combo (L→R / R→L / best-effort), recommended marker, kind flags (CONFLICT / BLOCKER / twin / blocked-parent), approve checkbox on requiresApproval rows. setItemWidget on tree rows — no delegate machinery.
- `SyncPreviewConsequencesDialog.h/.cpp` — consequences popup on explicit directory action: removed/changed per side + warnings from the Planner; Confirm applies, Cancel reverts.
- `SyncPreviewFakePairPicker.h/.cpp` — Stage-2-only stand-in "add pair" source listing canned fake scenarios (kitchen sink, deep tree ~500 rows to exercise load-more, empty) with auto pair ids; replaced by the real picker in Stage 5.

**Row cap + load-more (AC #5)**: per-pair display cap (200); "Load more (N remaining)" item appends the next batch.

**Approval gate (AC #3)**: commit enabled iff every requiresApproval row of the pair is approved (interpretation to confirm: flagged rows only, not literally every row).

**Demo launcher (AC #7, #8 verification)**: new test-gated target `src/MEGAAutoTests/SyncPreviewDialogDemo/` (CMakeLists + main.cpp) — QApplication, loads the queue file from QStandardPaths::AppLocalDataLocation/sync-preview-demo/, shows the dialog on the fake provider; closing/reopening the binary demonstrates persistence. Zero prod-file changes; launched via new justfile recipe `demo-syncpreview-dialog`.

**Unit tests** (`src/MEGAAutoTests/UnitTests/sync_preview/`): QueueFileStoreTests (round-trip, missing, corrupt), ReconcilerTests (drop/re-flag/drop-completed), PairControllerTests (add/remove, decisions, gating, persist-on-mutate), extend QueueStoreTests for schema v2 + v1 migration.

**Wiring (additive, 4 existing files)**: syncpreview.cmake (gui files + AUTOUIC path), UnitTests CMakeLists.txt (3 test files), MEGAAutoTests/CMakeLists.txt (demo subdirectory), justfile (demo recipe).

**Verification**: `just test` green; `just build` app target compiles (QT_NO_CAST_FROM_ASCII, -Werror=dangling-reference); `just demo-syncpreview-dialog` manual walkthrough of ACs #1–#8. No VER_FORK_SUFFIX bump (dev/test binaries only, per Stage-1 locked decision). No entry-point wiring (Stage 5).

Plan revisions from user review (2026-09-24): (1) Approval gate locked: commit enabled once every requiresApproval (flagged) row of the pair is approved — ordinary rows follow their overridable recommended action without explicit sign-off. (2) Launch strategy locked: prod hook now instead of a separate demo target — MegaApplication::showSyncPreviewDialog() beside showStalledIssuesDialog + a 'Review sync pairs' item in the infoDialogMenu (recreateMegaMenuAction pattern, sync-01 icon) beside the syncs group; DialogOpener::findDialog/showDialog for reopen; the user demos via `just run`. Wizard button + real commit flow stay Stage 5. Queue file: Preferences::instance()->getDataPath()/sync-preview-queue.json, resolved by the dialog's default ctor (matches prod singleton patterns); PairController still takes the path explicitly for unit tests. (3) Fake pairs across restarts: the injected pair-source factory maps persisted pair labels back to fresh FakeScenarios (factory is injected per construction; no fake knowledge in model/controller). (4) Approval survives action changes on the same row (the user made the change knowingly); re-flagging (approval reset) happens only when the classification itself changed on re-verify.
<!-- SECTION:PLAN:END -->

## Comments

<!-- COMMENTS:BEGIN -->
created: 2026-09-24 14:45
---
Testing feedback received 2026-09-24 (dialog functionally working; IA/readability rejected):
1. Terminal floods with repeated SDK fs.cpp mount-database DEBUG logs during `just run` (→ recorded in MEGA-3).
2. UI should be pair-first: list of pairs with summary details (size / file / dir counts), opening a pair opens a separate detail window; detail window must be a true Midnight-Commander-style dual pane with actions in the middle; keep the 'Show in-sync items' checkbox (→ recorded in MEGA-2.7).
3. Greyed-out/unselected rows hard to read with the current palette (→ also MEGA-2.7).

Consequence: MEGA-2.2 stays in Testing for now — its fate (Done vs rework inside 2.2) is the developer's call once MEGA-2.7 lands or the current shape is accepted. The rework ticket supersedes parts of AC #1/#6 as originally shaped.
---

created: 2026-09-24 14:48
---
Update 2026-09-24: per developer decision, the log-noise fix is tackled HERE (small fix after Testing → back to In Progress), and the standalone MEGA-3 ticket was archived (the stale 'MEGA-3' reference in comment #1 should read MEGA-2.2). Context carried over from MEGA-3:

Reported while testing the dev build (`just run`): the terminal floods with repeated SDK-side DEBUG logs. Pattern (repeats for every path check): `09/24-14:36:58.798609 <tid> DTL  Opening mount database: /proc/mounts [fs.cpp:2223]` / `... DTL  Path <path> is on device <device> [fs.cpp:2348]`. The [fs.cpp:...] tags indicate the MEGA SDK's internal logger, not the app's MegaLogger.

Investigation pointers (unverified — check first): where the app sets SDK log verbosity (search src/MEGASync for setLogLevel / setLoggerObject / logToConsole / MegaApi log setup in MegaApplication.cpp); likely levers: raise the SDK console threshold (e.g. to WARNING) while keeping DEBUG in the file — check whether file vs console levels are separately configurable here; or filter the specific repeated mount-database messages; check whether the noise is Debug-build-only and whether an env var / config knob already exists.

Constraints (fork policy): read-only by default, minimal surface, prefer dev-only or configuration-level fix; if the fix would change prod logging behavior, describe the change + risks and get explicit approval first. AC (added to this ticket): `just run` console no longer floods with the repeated fs.cpp mount lines (at most one occurrence per session), log-file verbosity not reduced (or tradeoff documented), build + tests stay green.
---

created: 2026-09-24 14:49
---
Correction 2026-09-24: MEGA-3 was removed entirely at the developer's request (no archived record remains). The log-noise context lives in comment #2 below and is being fixed in this ticket.
---
<!-- COMMENTS:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Stage 2 implemented: the pre-commit review dialog runs on the fake provider behind the Stage-1 seam, compiled into the dev app build with a "Review sync pairs" entry in the logged-in overflow menu (beside the Syncs/Backups group). Full suite green: 106 test cases, 612 assertions (just test).

What was built
- Model (sync_preview/model/): SyncPreviewQueueFileStore (file-backed persistence over the QueueStore codec; QSaveFile atomic write; missing/corrupt file → empty queue). SyncPreviewReconciler (restore-time re-verify: drops decisions for vanished rows, re-flags rows whose kind/approval-requirement changed by clearing approval, drops completed pairs). Queue schema v2: per-decision kind/requiresApproval snapshots + per-pair completed flag, with v1 migration (v1 snapshots read as UNKNOWN_KIND → re-flagged once on first restore). FakeScenarios::manyLocalOnlyFiles(count) for the load-more demo.
- GUI (sync_preview/gui/): SyncPreviewPairController (headless orchestrator: queue, per-pair classification via injected provider factory, decisions/approvals, plans via the Stage-1 Planner, approval gate = every requiresApproval row approved, persist-on-every-mutation, reconcile-on-restore). SyncPreviewDialog + .ui (tree grouped by pair, text filter, group-by-pair toggle, in-sync toggle, per-pair Commit gated on the approval state — commit flow itself is Stage 5). SyncPreviewRowWidget (action combo with recommended pre-marked and overridable, disabled on blocker rows, approval checkbox on flagged rows). SyncPreviewConsequencesDialog (directory-action popup: created/changed/removed per side + warnings, Apply/Cancel). SyncPreviewFakePairPicker (Stage-2 stand-in pair source: kitchen sink / 600-row long list / empty pair; the pair label doubles as the fake-data key so restored pairs re-scan to the same scenario).
- Prod hook (user-approved in plan review): MegaApplication::showSyncPreviewDialog() beside showStalledIssuesDialog + syncPreviewReviewAction menu item (recreateMegaMenuAction pattern, sync-01 icon) in the logged-in overflow menu. Queue file: <app data dir>/sync-preview-queue.json, resolved by the dialog default ctor; PairController takes the path explicitly for tests.
- Tests: SyncPreviewQueueFileStoreTests, SyncPreviewReconcilerTests, SyncPreviewPairControllerTests (round-trips, persistence across controller instances, re-flag on changed classification, vanish-drop, completed-drop, approval gate); QueueStoreTests extended for v2 round-trip, v1 migration and v2 structural violations.

Verification
- just build → app target compiles clean (QT_NO_CAST_FROM_ASCII, -Werror=conversion/switch/dangling-reference). just test → all 106 cases pass. Binary: build/dev/src/MEGASync/megasync (fresh).
- Machine-verified: #2 (decision/planner semantics via controller+planner tests), #3 gate logic, #8 persistence/reconcile logic, #7 (entire data path is the fake provider seam — no SDK/real calls anywhere in the module). Needs eyes: #1 dual-pane columns, #4 popup, #5 load-more, #6 grouping/filter.
- No VER_FORK_SUFFIX bump (dev/test binaries only, per Stage-1 locked decision). Known build side-effect: translation.source.ts regenerated by the build's lupdate pass with the new strings as unfinished entries.

To try it: quit the installed prod MEGAsync (single-instance lock), then `just run`, open the InfoDialog overflow menu → "Review sync pairs". Add pairs from the picker; decisions, approvals and pair add/remove persist across closing/reopening the dialog and app restarts (AC #8).

Follow-ups (recorded, none blocking): consequences popup could also fire on approval-less directory cascades; commit button click is a Stage-5 placeholder by design.

Post-Testing fix (2026-09-24, folded in from the removed MEGA-3 at the developer's request): squashed the dev-console log flood. Root cause: upstream Debug builds define LOG_TO_STDOUT (src/MEGASync/CMakeLists.txt `$<$<CONFIG:Debug>:...>`), which makes MegaSyncLogger mirror EVERY SDK log line to stdout — including the repeated fs.cpp mount-database checks ('Opening mount database: /proc/mounts' / 'Path ... is on device ...', LOG_LEVEL_MAX/DTL). File logging was never affected: MegaSyncLogger keeps full LOG_LEVEL_MAX verbosity in MEGAsync.log; the stdout mirror was the only lever. Fix: one-token change — removed LOG_TO_STDOUT from the Debug generator expression (runtime `--debug` flag in MegaApplication.cpp still ORs logToStdout on Linux, so the mirror remains opt-in). Behavior change, dev-build-only, flagged per fork policy: Debug builds now have a quiet console by default (`just run --debug` restores it); release/prod builds were never affected; no sync/transfer/update mechanism touched; upstream merge tax = one CMake token. Verified live: default run → 1 console line / 0 fs.cpp lines in 10s; `--debug` run → 1092 lines / 83 fs.cpp lines in 10s (opt-in works). just test still green (106 cases / 612 assertions); build clean. To use: `just run` for a quiet console, `just run --debug` when watching logs.
<!-- SECTION:FINAL_SUMMARY:END -->
