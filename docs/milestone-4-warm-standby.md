# Milestone 4 — Warm Standby & Parent Monitoring Implementation

Status: **implemented (v0.1)**

## Goal

Implement continuous monitoring and warm-standby maintenance while minimizing overhead.

## What is implemented

### Active parent health monitoring

- `EspHomeOpenThreadPlatformAdapter::read_parent_metrics()` reads:
  - parent RLOC16
  - parent average RSSI + last RSSI
  - parent link margin + age (derived from neighbor table)
- `ParentHealthMonitor` evaluates:
  - hard failure via `last_parent_rx_ms` (derived from neighbor age)
  - degraded link via average RSSI threshold + persist time
  - (control-plane threshold retained in schema; currently kept at 0 due to lack of public counters)

### Warm standby candidate maintenance

- Periodic neighbor-table scan (`neighbor_scan_interval`):
  - iterates router neighbors (excluding parent, excluding children)
  - applies `neighbor_max_age` filter
  - feeds candidates into `CandidateManager`

- Candidate scoring/ranking:
  - stable, cheap heuristic score from link margin + RSSI
  - hysteresis (`standby_replace_hysteresis`) avoids oscillation
  - freshness tracked via `standby_refresh_interval`

### Optional parent-search refresh

- `parent_search_refresh_interval` (disabled by default) calls:
  - `otThreadSearchForBetterParent()`

**Important:** OpenThread may automatically switch parents when this is invoked. This is an opt-in knob.

## Metrics / observability

Milestone 4 extends log-based diagnostics to include:

- Active parent quality:
  - average RSSI
  - link margin
  - age since last heard
- Standby candidate quality:
  - score
  - RSSI
  - link margin
- Standby freshness:
  - milliseconds since last candidate refresh

These are emitted by `DiagnosticsPublisher` only when values change.

## Configuration keys (new in Milestone 4)

- `neighbor_scan_interval` (time, default `10s`, set `0s` to disable)
- `neighbor_max_age` (time, default `60s`)
- `parent_search_refresh_interval` (time, default `0s`/disabled)

## Example

See: `examples/milestone-4-warm-standby.yaml`
