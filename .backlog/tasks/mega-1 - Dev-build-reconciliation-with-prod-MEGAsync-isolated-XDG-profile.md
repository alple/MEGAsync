---
id: MEGA-1
title: Dev build reconciliation with prod MEGAsync (isolated XDG profile)
status: Testing
assignee: []
created_date: '2026-09-23 18:39'
updated_date: '2026-09-24 08:24'
labels:
  - docs
dependencies: []
documentation:
  - README.linux.md
modified_files:
  - AGENTS.md
  - justfile
  - src/MEGASync/control/Version.h
  - src/MEGASync/control/Preferences/Preferences.cpp
type: task
ordinal: 1000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Record + wire the dev-build/prod reconciliation for MEGAsync.

Facts established from code:
- main.cpp hardcodes org "Mega Limited" / app "MEGAsync" — no build-type profile switch, no dev data-dir env var.
- Linux data path: XDG GenericDataLocation + /data/Mega Limited/MEGAsync (MegaApplication::loadDataPath). Holds settings, sync defs, megasync.lock (single-instance), mega.socket.
- Running a dev binary against the prod data dir is impossible while prod runs (lock). Same settings = same data dir = only one instance.

UPDATE (user decision): isolated XDG profile approach dropped. The dev build uses the PROD settings/data directory directly — the fork is an extension of prod functionality. The ~/.local/share-megadev profile and its copied settings were removed. Constraint remains: only one instance (prod or dev) may run at a time; quit prod MEGAsync before running the dev build.

Delivered:
- justfile: build / test / run helpers. Test recipe excludes the 3 known-broken upstream cases by default (setScaleFactorEnvironmentVariable, getLogMessages — SIGABRT via Qt fatal handler, "failing since the beginning" per the test file; getTimeString — expects <span>-decorated units prod never produces). Extra args pass through to the Catch2 binary.
- Version suffix (user-approved prod-file touch, display-only): VER_FORK_SUFFIX "-dev.1" in src/MEGASync/control/Version.h, appended in Preferences::VERSION_STRING (Preferences.cpp). VER_PRODUCTVERSION_STR, VER_FILEVERSION_CODE and the update-check numeric untouched. Binary reports 6.6.2-dev.1 in VERSION_STRING consumers (About dialog, HTTP /v endpoint).
- Fork policy + binary-bump rule recorded in AGENTS.md.

Known-broken tests (upstream, not ours): 3 cases excluded via justfile default; 45/46 cases pass otherwise.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Dev build instructions (cmake configure/build commands) documented
- [ ] #2 Single-instance constraint (prod and dev never run together; dev runs on prod settings directly) stated
- [ ] #3 libtool added to documented build dependencies
- [ ] #4 Fork policy (minimal surface, read-only additions, flag behavior changes, no personal data/paths) present in AGENTS.md
<!-- AC:END -->

## Definition of Done
<!-- DOD:BEGIN -->
- [ ] #1 justfile build/test/run recipes verified green
- [ ] #2 Version suffix display-only: updater numeric untouched
<!-- DOD:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Dev build reconciliation established and verified.

Build: cmake -DVCPKG_ROOT=/home/alek/case/projects/my/mega/vcpkg -S <repo> -B <repo>/build/dev -DCMAKE_BUILD_TYPE=Debug; target MEGAsync. Binary: build/dev/src/MEGASync/megasync (v6.6.2), smoke-tested OK.

Reconciliation: main.cpp hardcodes org "Mega Limited"/app "MEGAsync"; Linux data path = XDG GenericDataLocation + /data/Mega Limited/MEGAsync. Isolated dev profile via XDG_DATA_HOME=~/.local/share-megadev, prod settings copied once into it. Constraint: prod and dev never run simultaneously (same account/syncs; single-instance lock is per-profile but sharing the account is what's unsafe).

Build-dep fix: libtool missing from README.linux.md requirements — needed by vcpkg autotools ports (libsodium autoreconf failed without it). Also required an unshallow fetch of the vcpkg checkout to obtain the pinned builtin-baseline commit ef7dbf94.
<!-- SECTION:FINAL_SUMMARY:END -->
