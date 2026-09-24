---
id: MEGA-2.5
title: 'Decision: loser''s-copy policy on local-wins / remote-wins'
status: Done
assignee: []
created_date: '2026-09-24 10:32'
updated_date: '2026-09-24 11:05'
labels:
  - 'wayfinder:grilling'
milestone: Sync pre-commit review
dependencies:
  - MEGA-2.1
parent_task_id: MEGA-2
type: task
ordinal: 7000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Question

When a reviewed item gets a winner, what happens to the losing side's content?

For local-wins: the remote file is overwritten by upload; MEGA keeps the old remote file as a previous version automatically (ClaimOldVersion at putnodes) — unless the account has versions disabled, in which case the old node is moved to sync debris or removed (sync.cpp putnodes path; Preferences::fileVersioningDisabled() switches stalled-issue resolution to remote deletion). For remote-wins: the local file is overwritten by download — nothing versions the local disk; the existing stalled-issue implementation moves the local file to sync debris (Utilities::removeLocalFile), but pre-apply (if Route A) has no sync debris and needs its own backup location.

Decide the policy: is the loser always preserved (recommended: local loser backed up locally before overwrite; remote loser protected by MEGA versioning, with the versions-disabled case handled explicitly), and is the user informed or asked? Depends on the enforcement-route decision.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A resolution comment records the loser-copy policy for both directions (remote-wins overwriting local, local-wins overwriting remote)
- [ ] #2 The decision explicitly covers accounts with versioning disabled (fileVersioningDisabled / ATTR_DISABLE_VERSIONS path where the old remote node is removed instead of versioned)
- [ ] #3 The decision states where a backed-up local file lives (location, naming) and whether the user is informed/asked
<!-- AC:END -->

## Comments

<!-- COMMENTS:BEGIN -->
created: 2026-09-24 11:05
---
Resolution (2026-09-24, superseded by the directional-action model + user choice): **deletions are recoverable** — remote nodes go to MEGA's Rubbish bin (moveNodeToRubbish), local files go to OS trash or a backup folder, never a hard unlink. The consequences popup previews every removal before approval. (Original loser's-copy backup policy replaced by this rule; MEGA versioning still protects overwritten remote content where versions are enabled.)
---
<!-- COMMENTS:END -->
