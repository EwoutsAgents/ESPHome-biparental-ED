# Milestone 8 — Parent Switching Performance Comparison (plan only)

Status: **planned (not started)**

## Goal

Quantify parent-switching performance of:

1. **Reference baseline**: default Thread/OpenThread behavior (no biparental policy overlay)
2. **Novel approach**: `biparental_ed` active + warm-standby policy

The objective is to measure whether the biparental approach improves recovery/switch behavior under controlled conditions, and at what trade-offs.

## Scope

In scope:

- Parent-switch and reattach performance comparison on the same hardware setup.
- Timing, success-rate, and stability metrics.
- Scenario-based comparison (degradation/failure-triggered switching).

Out of scope (for this milestone):

- New failover algorithm changes.
- Production tuning changes to existing milestone behavior.
- Release packaging.

## Test variants

### Variant A — Default Thread reference

A comparable ESPHome image using standard OpenThread behavior only (without `biparental_ed` failover policy logic).

### Variant B — Biparental policy

ESPHome image with `biparental_ed` enabled and current milestone-7-validated behavior.

## Fair-comparison controls

- Same ED hardware type and firmware build settings where possible.
- Same router set and placement.
- Same channel/PAN/dataset.
- Same trigger scripts and disturbance schedule.
- Alternate run order (A/B/A/B...) to reduce drift bias.
- Repeated trials per scenario (target: at least 20, preferred 30+).

## Scenarios to compare

1. **Active parent outage**
   - Force active-parent unavailability (e.g., router reset/power cut).
2. **Link degradation path**
   - Trigger degraded conditions long enough to cross policy thresholds.
3. **Preferred-target unavailable / miss-like conditions**
   - Conditions where selected candidate cannot be used reliably.
4. **Steady-state sanity**
   - No forced disruption; ensure both variants maintain stable attachment.

## Metrics

Primary:

- **T_detach_to_child_ms**: time from detach event to regaining `Role detached -> child`.
- **T_failover_window_ms**: time from failover trigger/request to post-reattach stable state.
- **Switch success rate**: fraction of trials that reattach successfully within timeout budget.

Secondary:

- Parent-switch outcome distribution (success/miss/timeout where applicable).
- Failover reason distribution (`no_standby`, `preferred_miss`, `preferred_timeout`, etc.).
- Retry/attach-attempt counts during transition.
- Post-switch stability window (short-term re-detach frequency).

## Instrumentation and evidence

Planned sources:

- ESPHome serial logs with timestamps.
- Existing diagnostics lines (`state=...`, `preferred(...)`, `entity.last_failover_reason=...`).
- Trial metadata file: variant, scenario, run id, start/end markers.

Planned artifacts:

- Raw logs per trial set in `/tmp` and/or `artifacts/` paths.
- Parsed summary table (CSV/Markdown).
- Milestone report with median/p95 and pass/fail interpretation.

## Analysis plan

- For each scenario and variant, compute:
  - median, p90, p95 for timing metrics
  - success percentage
  - count of unstable outcomes (reattach loops / repeated detach)
- Compare A vs B using deltas and percentages.
- Report both wins and regressions (not only best-case numbers).

## Exit criteria

Milestone 8 is complete when all are true:

1. Both variants executed across defined scenarios with repeat count met.
2. Timing + success metrics are published in a reproducible summary.
3. Known anomalies and confounders are documented.
4. A recommendation is recorded:
   - keep current biparental policy as-is, or
   - tune specific parameters for better trade-off.

## Risks / caveats

- Serial/upload instability may affect throughput of repeated hardware runs.
- RF environment noise can bias absolute timings; controlled ordering is required.
- Baseline/reference config must remain truly comparable (avoid accidental policy leakage).

## Planned deliverables

- `docs/milestone-8-parent-switching-performance.md` (this plan, then results appended in a later phase)
- `docs/milestone-8-results.md` (optional, if separation is clearer)
- Scenario run logs + summary tables
- Final conclusion note in docs

## Next action (deferred)

Do **not** execute benchmark runs yet.

Next step, when approved: prepare comparable Variant A/B configs and a repeatable scenario runner, then begin trial collection.
