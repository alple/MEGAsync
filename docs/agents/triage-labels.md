# Triage Labels

> **Source of truth: the tracker is [Backlog.md](backlog.md)** — triage state is Backlog **statuses** + **labels**, not file status lines. **Board statuses are not wayfinder statuses** — wayfinder/triage roles ride as labels.

## Mapping triage roles → Backlog.md

Skills speak in the five canonical triage roles. When writing to Backlog, map them like this:

| Legacy role (skills) | Backlog.md | Meaning |
|---|---|---|
| `needs-triage` / `needs-info` | Status `Backlog` + label `needs-triage` | Fresh report: brief recorded, triage/grilling pass pending. Not current work; triage sorts: soon → `To Do`, later → stays `Backlog` |
| Status `Draft` | **Explicit developer request only** | The developer says "draft" when collecting a vague idea; agents never send reports to Draft on their own |
| `ready-for-agent` | Status `To Do` + label `ready` | Fully specified, takeable by an AFK agent |
| `ready-for-human` | The **`Testing` status** | Agent-finished, awaiting the developer's test/review = move the ticket to `Testing`; never self-`Done` (backlog.md → "Ticket flow") |
| `wontfix` | Do **not** action; record `WONTFIX` in the task description (Done tasks cannot be archived — the description is the record) | Will not be actioned |
| `implemented` / `resolved` / `done` | Status `Done` (+ `finalSummary` when the work itself is done in-session) | Finished — **records only**; live work never self-marks `Done` (Ticket flow) |
| `split` | Parent task carrying subtasks; parent mirrors children | Superseded by numbered follow-ups |

Live label vocabulary (workflow state, keep it small): `needs-triage`, `ready`. The **`Testing` status** (not a label) is the live agent-finished/awaiting-review signal — wayfinder's `ready-for-human` maps there. Labels are **not** the category — the task **type** (`bug`, `feature`, `enhancement`, `chore`, `docs`, `spike`, `task`, `epic`, `initiative`) is the only category marker; no `bug`/`feature` labels duplicating it. Cross-cutting labels (`refactoring`, `testing`, `docs`) stay available when they add meaning beyond the type (e.g. a bug whose fix is a repo-wide refactor). `epic` marks spec-carrying feature roots; `initiative` marks specless long-running containers (both in `.backlog/config.yml` `types` — parent kinds: backlog.md → "Two kinds of parents").

When a skill mentions a role (e.g. "apply the AFK-ready triage label"), use the corresponding string from this table.
