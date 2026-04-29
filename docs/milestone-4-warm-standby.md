# Milestone 4 — Warm Standby & Parent Monitoring Implementation

Status: **completed and hardware-validated (scope complete)**

## Goal

Implement continuous parent-health monitoring and warm-standby maintenance with low runtime overhead.

## Delivered in Milestone 4

### Active parent health monitoring

- `EspHomeOpenThreadPlatformAdapter::read_parent_metrics()` now provides:
  - parent RLOC16,
  - parent average/last RSSI,
  - parent link margin,
  - parent age (from neighbor table).
- `ParentHealthMonitor` classifies `HEALTHY` / `DEGRADED` / `FAILED` using:
  - hard-timeout policy,
  - degraded-RSSI persist policy,
  - control-plane threshold config (currently wired but limited by OT public counters).

### Warm standby candidate maintenance

- Periodic router-neighbor scan (`neighbor_scan_interval`) excluding:
  - active parent,
  - child entries.
- Candidate freshness filtering (`neighbor_max_age`).
- Candidate ranking with hysteresis (`standby_replace_hysteresis`).
- Standby staleness lifecycle (`standby_refresh_interval`).
- Continuity fallback: on active-parent change, previous active parent is promoted to standby candidate (`CandidateManager::mark_failover_complete()`).

### Failover integration wiring

- Generic and preferred failover requests are wired through OT adapter calls.
- Preferred path remains best-effort (Thread/OpenThread parent selection behavior).
- Preferred requests can rank a target standby, but public OpenThread APIs still own the actual parent decision; there is no deterministic RLOC16 pinning path.

### Diagnostics / observability

`DiagnosticsPublisher` includes:
- failover/health state,
- active + standby RLOCs,
- active RSSI/link-margin/age,
- standby score/RSSI/link-margin/freshness,
- failover counters,
- preferred-failover counters (`attempt/success/miss`).

## Hardware validation summary

Validation log: `/tmp/biparental-ed-validation-8.log`

Observed during disruption tests:
- standby acquisition became valid (`standby=yes` with active/standby RLOCs),
- preferred failover path issued and accepted,
- reattach transitions observed with failover counter progression.

## Milestone 4 scope boundary

Milestone 4 is complete for:
- monitoring,
- standby maintenance,
- runtime diagnostics,
- baseline failover actuation plumbing.

Milestone 5 is still required for deterministic preferred-target outcome control (policy/behavior beyond best-effort request issuance).

## Next document

For immediate continuation, use:
- [`docs/milestone-5-handoff.md`](milestone-5-handoff.md)
