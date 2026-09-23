# AGENTS.md (for AI agents & humans)

## Precedence (repo docs override skills)

Skills (`.agents/skills/`) may be overwritten or reinstalled — conveniences, **not** the source of truth. On conflict, repo files win:

1. `AGENTS.md` (this file) — always loaded
2. `docs/agents/backlog.md` — canonical tracker spec
3. `docs/agents/*.md` (issue tracker, triage labels, domain docs)

Follow the repo file and note the override.

## Agent skills
Load skills from: .agents/skills

### Issue tracker

**Source of truth: [Backlog.md](docs/agents/backlog.md) via the `backlog` MCP server (data in `.backlog/`, git-tracked).** All new issues, epics, milestones, and comments go there — via the backlog MCP tools, never hand-edited `.backlog/` files (config exception: `statuses`/`types`/`labels` in `.backlog/config.yml` are edited directly; restart the MCP server after — it caches both lists).

- **Overrides the skills**: "publish to `.scratch/`", `Status:` lines, wayfinder maps there → Backlog tasks instead (wayfinder maps = route epic + decision-ticket children with native `dependencies`); mapping: `docs/agents/issue-tracker.md` → "Where skills' tracker instructions map now". Don't "fix" the skills — upstream.

### Triage labels

Triage state = Backlog **statuses + labels**, not file status lines; **board statuses are not wayfinder statuses** — roles ride as labels. Mapping: `needs-triage`/`needs-info` → `Backlog` + label `needs-triage` (fresh reports aren't current work; triage sorts: soon → `To Do`, later stays); `ready-for-agent` → `To Do` + label `ready`; `ready-for-human` → the **`Testing` status** (label retired to records); `implemented`/`resolved`/`done` → `Done` (records only — live work never self-marks `Done`); `wontfix` → `WONTFIX` in the description. `Draft` only on the developer's explicit "draft". Labels = workflow state only (`needs-triage`, `ready`; cross-cuts `refactoring`/`testing`/`docs`) — the task **type** is the only category marker; no `bug`/`feature` labels duplicating it. Full table: `docs/agents/triage-labels.md`.

### Task lifecycle (never auto-Done)

Canonical spec: backlog.md → "Ticket flow". The gates:

- **Create:** find a suitable epic (open or closed) first — found → ask attach-or-new; not found → standalone. Status explicit: current/soon → `To Do`; future/reminder or fresh report → `Backlog` (+ `needs-triage` for reports). Uncertain → ask.
- **Spec:** speccing a `Backlog` ticket moves it to `To Do` (speccing = soon).
- **Start:** claim (assign) + `In Progress` before any work.
- **Finish:** → **`Testing`** + completion summary (`finalSummary` or comment) — **never `Done`** (overrides the backlog MCP task-finalization guidance; repo docs > tools/skills).
- **`Done` is the developer's call**, per task, on explicit instruction after testing. Agents may ask; never move unilaterally, never `Testing` → `Done` automatically.
- **Fixes after Testing:** small → same ticket (`Testing` → `In Progress`); extensive → propose a new ticket; unclear → ask.
- **Two parents:** feature root (`epic`, carries the spec, mirrors children) vs `initiative` (specless long-running container, parked `To Do`, no mirroring, retired by hand = archived). Mirroring: parent = least-advanced child stage, capped at `Testing`; a fresh child on a `Done` root reopens it.
- **Wayfinder decision tickets exempt:** resolving a map ticket closes it (`Done`) — the deliverable is the recorded decision, shaped live in grilling.
- **Soft frontier:** blockers all in `Testing`/`Done` → startable (treat as satisfied for planning; note it builds on unreviewed work). Native `ready` filter still requires `Done` — the developer's hard gate.
- **Exceptions (records only):** `wontfix` records land as `Done` + `WONTFIX` in the description (Done tasks cannot be archived — the description is the record).

### Domain docs

Single-context layout — one `CONTEXT.md` + `docs/adr/` at the repo root. See `docs/agents/domain.md`.

<!-- BACKLOG.MD MCP GUIDELINES START -->
<!-- backlog.md-instructions-version: 1.51.0 -->

<CRITICAL_INSTRUCTION>

## BACKLOG WORKFLOW INSTRUCTIONS

This project uses Backlog.md MCP for all task and project management activities.

**CRITICAL GUIDANCE**

- If your client supports MCP resources, read `backlog://workflow/overview` to understand when and how to use Backlog for this project.
- If your client only supports tools or the above request fails, call `backlog.get_backlog_instructions()` to load the tool-oriented overview. Use the `instruction` selector when you need `task-creation`, `task-execution`, or `task-finalization`.

- **First time working here?** Read the overview resource IMMEDIATELY to learn the workflow
- **Already familiar?** You should have the overview cached ("## Backlog.md Overview (MCP)")
- **When to read it**: BEFORE creating tasks, or when you're unsure whether to track work

These guides cover:
- Decision framework for when to create tasks
- Search-first workflow to avoid duplicates
- Links to detailed guides for task creation, execution, and finalization
- MCP tools reference

You MUST read the overview resource to understand the complete workflow. The information is NOT summarized here.

</CRITICAL_INSTRUCTION>

<!-- BACKLOG.MD MCP GUIDELINES END -->
