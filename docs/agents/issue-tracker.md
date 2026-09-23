# Issue tracker: Backlog.md

> **Source of truth: [Backlog.md](backlog.md)** via the `backlog` MCP server (data in `.backlog/`, git-tracked). All new issues, epics, milestones, and comments go there. This file records how skills' tracker instructions map onto it.

## Where skills' tracker instructions map now

Skills still speak in local-markdown terms (feature directories, `.scratch/` buckets, `Status:` lines, `spec.md`, wayfinder maps) or GitHub issues (`gh issue create`). Interpret them through the repo docs, which win:

| Skill instruction | Do this instead |
|---|---|
| "Publish to the issue tracker" / create a feature dir | Create Backlog.md task(s) — feature root (type `epic`, spec-carrying) + subtasks as needed; `initiative` for specless long-running containers (backlog.md → "Ticket flow") |
| "Fetch the relevant ticket" | `task_view` / `task_search` on the Backlog task |
| "Open a GitHub issue" / `gh issue create` | Backlog task instead — this repo's tracker is local, not GitHub Issues |
| Triage role labels (`needs-triage`, `ready-for-agent`, …) | Map through `docs/agents/triage-labels.md` (statuses + labels in Backlog) |
| Wayfinder map/child tickets | Backlog feature root (`wayfinder:map` label) + child tasks; blocking via native `dependencies`; decision tickets close `Done` on resolution (lifecycle exemption) |
| Wayfinder `ready-for-agent` / `ready-for-human` | Labels, not board statuses: `ready` (= ready-for-agent); ready-for-human = the **`Testing` status** |

**Task lifecycle (2026-09-23):** fresh reports → `Backlog` + `needs-triage`; speccing moves a ticket to `To Do`; work starts `In Progress` (claim + status) and finishes at **`Testing`** (completion summary) — agents never set `Done`; the developer does, per task, after testing. See backlog.md → "Ticket flow", AGENTS.md → "Task lifecycle".
