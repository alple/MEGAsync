# Tracker: Backlog.md — capabilities, statuses, and the ticket flow

Backlog.md is this repo's task tracker, used through the `backlog` MCP server (data in `.backlog/`, git-tracked; CLI equivalent: `npx -y backlog.md …`). **This file is the canonical spec for statuses, grouping, organisation, and the ticket flow** — it wins over skill/resource suggestions, incl. the MCP's task-finalization guidance. Task lifecycle/execution discipline otherwise follow the MCP resources (`backlog://workflow/overview` + task-creation / task-execution / task-finalization).

> Board and types configured **2026-09-23** (initial setup, carried over from the cat-app board's proven values). Re-verify on major backlog.md bumps.

## Entities — one dimension per mechanism

| Question | Mechanism |
|---|---|
| *What kind* of work? | **Type** (`bug`, `feature`, …, `epic`, `initiative`) — the only category marker |
| *What workflow state?* | **Label** (wayfinder/triage roles, cross-cuts) |
| *How broken down?* | **Parent task + subtasks**, or separate tasks + dependencies |
| *When does it land?* | **Milestone** (sprint/release bucket) |
| *Where on the board?* | **Status** (`Backlog`, `To Do`, `In Progress`, `Testing`, `Done`) |

Board statuses are **not** wayfinder statuses: `ready-for-agent` rides as the `ready` **label**; `ready-for-human` maps to the **`Testing` status** (see [Ticket flow](#ticket-flow-the-statuses-in-motion) and `docs/agents/triage-labels.md`). Long-form intent (spec, PRD) → **document** (type `specification`), linked from the feature root's `documentation`/`references`.

## Verified capabilities

- **Nesting:** subtasks ≥2 levels deep (`MEGA-1 → MEGA-1.1 → MEGA-1.1.1`); dotted IDs (`MEGA-N.M[.K]`); tree rendered via `Parent:`/`Subtasks:`. `parentTaskId` takes an existing **task** ID, never a milestone ID.
- **Labels:** colon hierarchies work (`group:refactoring` round-trips; filters via `task_list(labels=["group:refactoring"])`); string, max 50 chars.
- **Milestones:** unique name + optional due date (`YYYY-MM-DD`); filter via `task_list(milestone=...)`. Nothing auto-enforced — a grouping label.
- **Statuses:** user-defined — configured **2026-09-23** as `["Backlog", "To Do", "In Progress", "Testing", "Done"]`, `default_status: "Backlog"`. `task_list(ready: true)` = dependencies Done. `Draft` is not in the configured list but works as a hidden status (drafts live in `.backlog/drafts/`, excluded from normal listings) — only when the developer explicitly says "draft".
- **`statuses` and `types` are config-only** — keys in `.backlog/config.yml`, settable like `labels` (edit the config file directly; no `config set`). Both configured 2026-09-23 from day one. **A long-running MCP server caches both lists at startup — restart the backlog MCP server after changing either**, or MCP writes/filters with the new value fail validation (CLI reads stay fine). *Verified 2026-09-15 on cat-app (same backlog.md version): after a statuses change and before an MCP restart, an MCP write with `status: Testing` failed validation ("must be one of: Draft, To Do, In Progress, Done") while `npx -y backlog.md config get statuses` returned the new set.*

## Ticket flow (the statuses in motion)

Canonical lifecycle for every task. Board statuses are **not** wayfinder statuses — legacy roles map through `docs/agents/triage-labels.md`.

### Status meanings

| Status | Meaning | Who moves it there |
|---|---|---|
| `Backlog` | Longer-term work, recorded reminders, fresh untriaged reports (`Backlog` + label `needs-triage`) — not current | Agent at creation (rule of thumb) |
| `To Do` | Current work done soon. Default landing for new tickets; fully specced and takeable → label `ready` | Agent at creation / when speccing |
| `In Progress` | Claimed and being worked | Agent, on start |
| `Testing` | Agent-finished, awaiting the developer's test/review — the live signal | Agent, on finishing |
| `Done` | The developer tested and accepted | **The developer only — never automatic** |

`Draft` stays as before: only on the developer's explicit "draft" (a vague idea being collected) — agents never send reports to Draft.

### Creating a ticket

1. **Find the parent first.** Search for a suitable epic (feature root) — open **or closed**. Found → **ask: attach it there or create new?** Not found → standalone.
2. **Route escalation:** adjustment/follow-up tickets for one feature clustering as standalones → create a fresh **feature root** (type `epic`) carrying the spec, group them under it.
3. **Set status explicitly at creation** — never rely on defaults. Current work done soon → `To Do`; clearly future reminder → `Backlog`. **Uncertain → ask.** Fresh untriaged reports → `Backlog` + `needs-triage` (triage sorts: soon → `To Do`, later → stays).

### Two kinds of parents

- **Feature root (type `epic`):** carries the feature's **spec** (linked document or in-description); the route for the feature and its rounds of adjustments — each round a child ticket. It is itself a work item: moves through the flow and **mirrors its children** (below). Wayfinder maps hang off it (map = route epic + decision-ticket children with native `dependencies`).
- **Initiative (type `initiative`):** a specless **long-running container** for related but independently specced tasks (e.g. a big dependency migration: SDK swap + facade migration + test repair + verification, each task its own spec). Status semantics **ignored**: sits in `To Do` for its whole life, excluded from mirroring and close-out, retired by hand (`task_archive`). Rule of thumb: **an `epic` must carry a spec; a parent with no spec of its own is an `initiative`.** *(Rationale: containers without an end state must not carry workflow semantics — the industry-standard epic-vs-initiative/theme distinction.)*

### Parent mirroring (feature roots only)

Root status = **least-advanced child stage, capped at `Testing`** (`To Do` < `In Progress` < `Testing` < `Done`): any child not started → root `To Do`; any child `In Progress` → root `In Progress`; all children past `In Progress` → root `Testing`; all `Done` → root stays `Testing` (close-out is the developer's call). Consequence: a fresh child on a `Done` root **reopens the root** — no manual resurrection of closed epics. Initiatives never mirror.

### Lifecycle

- **Speccing:** asked to spec out a `Backlog` ticket → move it to `To Do` when the spec is done (or while speccing) — speccing means it will be worked soon.
- **Start:** claim it (assign) + `In Progress` before any work.
- **Finish:** → **`Testing`** + completion summary (`finalSummary`, or a comment). **Never `Done`** — overrides the backlog MCP task-finalization guidance (repo docs > tools/skills).
- **Testing → Done is the developer's.** After testing they report fixes or move it themselves. Agents may **ask** whether a finished task should be marked `Done`; never move unilaterally.
- **Fixes after Testing:** fold back into the same ticket (`Testing` → `In Progress`, then finish → `Testing` again); extensive → **propose a next ticket** (same feature root); unclear → **ask: implement now or new ticket?**
- **Wayfinder decision tickets are exempt:** resolving a map ticket closes it (`Done`) — its deliverable is the recorded decision, shaped live in grilling. Implementation tickets off the map follow the normal flow.
- **Soft frontier:** a task whose blockers all sit in `Testing`/`Done` may be started — treat blockers as satisfied for planning (work complete, review/testing pending), note it builds on unreviewed work. The native `ready` filter still requires `Done`; that hard gate is the developer's.
- **Exceptions (records only):** `wontfix` records land as `Done` + `WONTFIX` in the description (Done tasks cannot be archived — the description is the record).

## Grouping conventions

- **Labels carry workflow state and themes** (`needs-triage`, `ready`, cross-cuts `refactoring`/`tech-debt`/`docs`). Timing/status live in status and milestones, not labels.
- **Parents are the two kinds above** (`epic` = spec-carrying feature root; `initiative` = specless container). Keep the hierarchy shallow (children are work orders; grandchildren rare).
- **Subtasks vs. separate tasks:** subtasks for tightly coupled work on the same component; separate tasks wired with `dependencies` (state what each provides) for independent cross-component work parallelizable across sessions.
- **Milestones are time buckets only** — one per sprint/release, tasks assigned when scheduled; mixes refactorings and features freely. **Anti-pattern:** a permanent theme-milestone ("Refactoring") — duplicates the label's job and pollutes sprint planning.
- **`Draft` = parked vague idea**, developer-explicit opt-in only (hidden from normal listings). Everything else parks in `Backlog`.
- **Sizing:** one task = one focused PR. Ten acceptance criteria = a task that wants splitting.

Never edit `.backlog/` markdown directly — all changes via MCP tools (or the `backlog` CLI) so IDs, relationships, and history stay consistent.

## Verification

2026-09-23, initial setup: probe-confirmed on this repo — three-level chain (`MEGA-1 → MEGA-1.1 → MEGA-1.1.1`), colon-label round-trip and `task_list` label filter, `Testing` status write via CLI, then all probe tasks archived (IDs reusable). Load-bearing facts: the nesting chain, colon labels, and the status enum. Re-run the probe if backlog.md changes materially (cat-app's identical board verified the status-enum cache behaviour 2026-09-15, see Verified capabilities).
