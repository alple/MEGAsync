---
id: MEGA-2.9
title: >-
  Stage 2.5 UI evolution: rename-aware conflict resolution + show-changes/apply
  review loop
status: Testing
assignee:
  - '@agent'
created_date: '2026-09-25 16:09'
updated_date: '2026-09-25 20:00'
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

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
## Implementation plan (recorded before implementation, 2026-09-25)

Builds on committed MEGA-2.8 state (7b6fb0d, clean tree). All changes inside `src/MEGASync/sync_preview/` + its unit tests; nothing upstream-touched.

### 1. Model: rename-aware operation vocabulary (Defs/Planner)
- `SyncPreviewDefs.h`: add `OperationType::RenameRemote`, `RenameLocal` (real-API basis: remote rename = `MegaApi::moveNode` rename/move; local = rename into trash-safe placement — flagged for Stage 4 verification, no API calls in Stage 2).
- `SyncPreviewPlanner.h`: `PlannedOperation` gains `fromPath`/`toPath` (rename ops carry old→new; `path` stays the row path). `RowPlan` gains rename consequences `renamedRemote`/`renamedLocal` (`QPair<QString,QString>` from→to); `addConsequence` extended. Renames add no bytes/removals to `PairSummary` (documented).
- Planner rename-aware expansion of →/← (decision-level, per ticket):
  - **Conflict local-side row** L→R = adopt: `RenameRemote(twinPath → rowPath)` (replaces naive Upload + duplicate warning). R→L stays `DeleteLocalToTrash` (content preserved at remote twin). BestEffort keeps plain transfer + duplicate warning.
  - **Conflict remote-side row** R→L = adopt: `RenameLocal(twinPath → rowPath)`; L→R stays `DeleteRemoteToRubbish`; BestEffort keeps plain download + warning.
  - **Paired BothDiffer row with twin** (renameSwap fixture): L→R = adopt `RenameRemote(twin → rowPath)` + displace the in-path remote entry: rename it to the vacated twin path when its content is unique (the swap), else `DeleteRemoteToRubbish` when content preserved elsewhere. R→L symmetric when the destination side holds the remote content under another name; otherwise plain replace. Apply order: displacement rename first.
  - **Blocker rows** become decidable: → = displace the in-path destination-side entry by auto-rename to first free `name (N).ext` (case-insensitive uniqueness), then the transfer (Upload/Download; after displacement the path is free, so create-not-replace). Type-mismatch folder displacement renames the whole subtree. BestEffort stays disabled on blockers. Deterministic name algorithm in the planner; UI notes spell it out.
  - Twin-row coupling: when the local-side twin row carries an explicit transfer decision, the remote-side twin row is covered by it (coveredByPath + superseded note); vice versa when only the remote side decides.

### 2. Model: classifier twin detection for PAIRED rows
- New pass after Pass C: for each paired BothDiffer file row, match `row.local` content against unconsumed remote leftovers → set advisory `hasIdenticalTwin`/`twinPath` on the paired row AND on the leftover row (leftover keeps `RemoteOnly`; flag no longer Conflict-only — struct comment updated). Symmetric direction: `row.remote` content vs still-LocalOnly placeholder rows. First match only (single `twinPath`, documented limitation). No `requiresApproval` added (advisory).

### 3. Model: fake apply engine (new `SyncPreviewFakeApplier.{h,cpp}`)
- `applyPlan(FakeScenario&, const Plan&)`: executes scheduled ops on the fake trees — Upload/Download create the entry on the destination side (source entry data, missing ancestor folders auto-created); Upload/DownloadReplace overwrite entry data; DeleteRemote/Local remove (subtree for folders); RenameRemote/Local move entry old→new (ancestors created, no pruning); cross-renames applied via temp path (swap-safe). Subtree ops expand to descendants. Pure model function, unit-tested.

### 4. Controller (`SyncPreviewPairController.{h,cpp}`)
- **Gate fix (engine-side)**: `awaitingApprovalCount` skips rows whose effective action is None **via explicit decision** (own or inherited from a directory decision); undecided conflict/blocker rows (recommended None, no decision) still count. `allApproved` follows.
- **Apply seam**: `setPlanApplier(std::function<bool(pairId, const Plan&)>)` + `applyPlan(pairId)`: compute plan → applier mutates fake data → rescanPair → reconcile that pair via `Reconciler` (vanishing rows drop their decisions) → persist → `pairChanged`. No approval gate on Apply (rehearsal loop: apply → inspect → adjust → apply again; undecided flagged rows simply contribute no ops).

### 5. GUI
- `SyncPreviewDialog`: per-pair member fake-tree store `QHash<QString, FakeScenario>` (label-keyed, seeded lazily from the canned scenario, session-only) shared by the factory + applier lambdas; per-pair-row **Show changes** outline button.
- New `SyncPreviewChangesDialog` (mirrors ConsequencesDialog structure/theming): flat scheduled-changes list from the Plan — per row: action prose, path, from → to for renames, change kind (created/overwritten/renamed/removed, per side); rows still awaiting approval marked in warning color. Opened from pair list rows and the detail window.
- `SyncPreviewPairDetailDialog`: **Show changes** + **Apply (fake data)** outline buttons in the filter row; blocker transfer buttons enabled with rename-aware tooltips; notes/tooltips updated (no more "arrives in a later stage" for transfers; notes spell out the rename resolution, e.g. displace target name); conflict-row tooltips mention the adopt-rename. Approve-toggle visibility logic unchanged (already effective=None-aware).

### 6. Scenarios + tests
- New canned scenarios + picker entries: `renameChain` (double rename, crossing), `renameEdit` (rename + independent edit), `renameCrossFolder` (paired-differ with twin across folders); kitchen sink gains one nested rename-twin case (densification).
- Tests: planner rename-aware ops (conflict adopt, paired swap displacement, blocker displace+transfer, coupling/supersede), classifier paired-twin flags (renameSwap fixture + siblings), applier round-trips (apply → re-classify → rows vanish/turn Same/identical), controller gate fix + apply loop reconcile. New `SyncPreviewFakeApplierTests.cpp`; extend planner/classifier/provider/controller test files.

### File inventory
model: Defs.h, Planner.{h,cpp}, Classifier.{h,cpp}, FakeSyncPreviewProvider.{h,cpp}, new FakeApplier.{h,cpp}; gui: PairController.{h,cpp}, Dialog.cpp, PairDetailDialog.{h,cpp}, new ChangesDialog.{h,cpp}, FakePairPicker.cpp (labels), syncpreview.cmake; tests: Planner/Classifier/Provider/Controller test files + new applier tests.

### Order
Defs/Planner → Classifier → Applier → Controller → GUI → scenarios → tests → `just build` + `just test` green → finish (Testing + summary).

### Risks/notes
- Persistence schema unchanged (Action enum untouched; Reconciler handles vanished rows on restore).
- Consumed-twin edge (twin content also exact-paired elsewhere): no twin hint, naive replace — pinned by a test as documented v1 behavior.
- Rename op real-API basis (moveNode) recorded for Stage 4/5 verification; no SDK/upstream changes now.
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Implemented rename-aware conflict/blocker resolution + the show-changes/apply review loop (Stage 2.5, fake data only). `just build` clean, `just test` green: **904 assertions / 133 cases** (baseline 652/109). Diff: 20 files, +1471/−87, all inside `sync_preview/` + its tests + the auto-regenerated `translation.source.ts`; nothing upstream-touched, no real API calls anywhere.

**1. Rename-aware →/← (decision-level, as confirmed):**
- New operation types `RenameRemote`/`RenameLocal` (real-API basis: remote = `MegaApi::moveNode` rename/move; flagged in the plan for Stage 4 verification). `PlannedOperation` carries `fromPath`/`toPath`; `RowPlan` carries `renamedLocal`/`renamedRemote` (from→to pairs); renames transfer no bytes, so PairSummary ignores them.
- **Conflict rows:** the arrow adopts the identical twin by renaming it to the row's name (no duplicate transfer, no bytes); the opposite arrow stays the content-safe naive delete; best-effort keeps the plain transfer + duplicate warning. An adopt **covers** its twin row (coveredByPath; superseded warning if that row had an explicit decision) — when both twins carry explicit decisions, the earlier (local-side) row's adopt wins deterministically.
- **Paired rows with a twin** (the `renameSwap` fixture): → becomes the rename-aware swap — displacement rename of the in-path entry into the twin's vacated name (or to Rubbish when its content survives elsewhere on that side), then the adoption; ops batch-apply atomically so neither copy is lost.
- **Blocker rows are now decidable:** → displaces the destination-side entry by automatic rename (first free `name (N)[.ext]`, case-insensitive; folders keep their whole name) and transfers onto the freed path. Best-effort stays disabled (a merge cannot fix a structural collision); tooltips and the notes line spell out the exact rename once decided.
- **Gate fix (engine-side):** `awaitingApprovalCount`/`allApproved` skip rows whose effective action is None **via explicit decision** (own or inherited from a directory decision); undecided conflict/blocker rows (recommended None) still block the commit.

**2. Classifier twin detection for paired rows (Pass D):** advisory `twinPath` (remote counterpart of the local content) and new `localTwinPath` (local counterpart of the remote content), set on the paired row AND on the counterpart row, first-match only (a leftover/placeholder serves at most one paired row). Flagged advisory only — no `requiresApproval` change, no kind change.

**3. Show changes / Apply loop:** new `SyncPreviewChangesDialog` (path + scheduled change per operation, renames as `from → to`, subtree notes, awaiting-approval rows in warning color, aggregated planner warnings) opened from the pair list rows and the detail window. New model-level `SyncPreviewFakeApplier::applyPlan(FakeScenario&, const Plan&)` executes the op vocabulary on the fake trees (copies with ancestor creation, replaces, subtree deletions, renames; same-row swap pairs apply as one atomic permutation). `PairController::setPlanApplier` + `applyPlan(pairId)`: applies, re-scans, re-verifies via Reconciler (vanished rows drop their decisions), persists, emits `pairChanged` — the panes re-resolve (applied rows vanish or turn Same). Apply is **not** gated on approvals (rehearsal loop, as confirmed); undecided flagged rows contribute no ops. The fake trees live in a session-only store in `SyncPreviewDialog` (label-keyed, seeded from the canned scenario); on reopen the queue re-scans to the original scenario and reconcile re-flags.

**4. Fake-data depth:** new pickable scenarios `renameChain` (double rename, both directions), `renameEdit` (rename + edit — pins that NO bogus swap is offered; instead the plain replace carries a new unique-content advisory: "replacing X discards content not preserved anywhere else on this side"), `renameCrossFolder`; the kitchen sink gained a nested rename-twin branch (`projects/mega/client/backup/main.cpp`) placed before the twins so the pinned leftover tail order held.

**ADJUSTMENT BEYOND THE RECORDED PLAN (flagging for review):** twin-flagged single-sided rows — and single-sided directories whose subtree contains one — now recommend **None** instead of a naive transfer. Without this the recommended plan fought a decided adopt (the apply-loop tests exposed it: the leftover twin's recommended download duplicated the displaced content and masked the fresh twin pair). Conflicts keep the existing warn-and-cascade behavior; the arrows remain available, so this only removes an unhelpful default. If you dislike it, the fallback is keeping the naive recommendation — say the word and I'll flip it.

**Notes for Testing (human eyes):**
- Pick `Demo: rename swap` → select `foo.txt` → press `→` → notes should spell the swap; `Show changes` lists two renames; `Apply` → `foo.txt` turns Same (hidden by default), `bar.txt` re-appears holding the remote edit (content preserved). Then `←` on `bar.txt` or leave it.
- `Demo: kitchen sink` → the `notes/A.txt` and `misc/notes.txt` blocker rows now accept the arrows; notes/tooltip state the displaced name (`a (1).txt` / `notes.txt (1)`); best-effort stays greyed there.
- `Demo: rename chain` → `foo.txt` shows two twin hints; either arrow resolves; after Apply the two leftover names surface as a fresh conflict pair for the next round (pin: no silent duplication).
- Commit-gate: pick `Do nothing` on a flagged row → the awaiting count drops; leave a conflict untouched → it still blocks.
- Renames show zero bytes in the pair summary's pending delta (by design).
<!-- SECTION:FINAL_SUMMARY:END -->
