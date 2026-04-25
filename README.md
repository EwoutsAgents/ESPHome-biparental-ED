# ESPHome-biparental-ED

ESPHome external component for ESP32-C6 to implement **active-passive bi-parental behavior** for a Thread End Device (ED):

- one **active parent** used for all normal traffic,
- one **warm standby parent candidate** tracked and refreshed in the background,
- accelerated reattachment when active-parent quality becomes unacceptable.

This project intentionally stays aligned with Thread/OpenThread attachment behavior and does **not** assume direct failover from child context alone.

---

## Milestone 1 — Problem Definition & Architecture Freeze (completed)

Milestone 1 deliverable note:

- [`docs/milestone-1-architecture-freeze.md`](docs/milestone-1-architecture-freeze.md)

Milestone 1 outcome:

- Architecture and behavior definition frozen as active-passive overlay design.
- Parent selection and failover state machine defined.
- Failure detection and anti-flap policy defined.

---

## Milestone 2 — Protocol-Level Feasibility Validation (completed)

Milestone 2 deliverable note:

- [`docs/milestone-2-protocol-feasibility.md`](docs/milestone-2-protocol-feasibility.md)

Milestone 2 outcome:

- Confirms feasibility of active-passive warm-standby behavior as an **external overlay**.
- Freezes compliance boundaries for what can be implemented without stack modifications.
- Explicitly separates compliant behavior from cases that would require OpenThread internal changes.

---

## Milestone 3 — ESPHome External Component Design (completed)

Milestone 3 deliverable note:

- [`docs/milestone-3-external-component-design.md`](docs/milestone-3-external-component-design.md)

Milestone 3 outcome:

- External component scaffold added under `components/biparental_ed/`.
- Internal modules defined:
  - ParentHealthMonitor
  - CandidateManager
  - FailoverController
  - DiagnosticsPublisher
- Internal failover state machine scaffold implemented.
- YAML configuration schema and example added.

---

## Installation (external_components)

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/EwoutsAgents/ESPHome-biparental-ED
      ref: main
      path: components
    components: [biparental_ed]
```

## Example YAML

See [`examples/milestone-6-observability.yaml`](examples/milestone-6-observability.yaml).

```yaml
biparental_ed:
  id: biparental_controller
  update_interval: 1s
  degraded_rssi_threshold: -72
  hard_failure_timeout: 15s
  degrade_persist_time: 10s
  hold_down_time: 30s
  standby_refresh_interval: 60s
  failover_eligible_delay: 10s
  post_failover_stabilize_time: 10s
  preferred_reattach_timeout: 5s
  enable_parent_response_callback: true
  control_plane_error_threshold: 3
  standby_replace_hysteresis: 5
  neighbor_scan_interval: 10s
  neighbor_max_age: 60s
  parent_search_refresh_interval: 0s

  runtime_debug_enabled: true
  runtime_debug_interval: 5s
  verbose_diagnostics: true
  diagnostics_publish_interval: 5s

  active_parent_id:
    name: Active Parent ID
  standby_parent_id:
    name: Standby Parent ID
  failover_count:
    name: Failover Count
  degraded_state:
    name: Parent Degraded
  last_failover_reason:
    name: Last Failover Reason
```

### Observability notes

- All policy timers/thresholds remain available as YAML options.
- ESPHome diagnostic entities are optional; omit them to stay backward compatible.
- Runtime debug logging and diagnostic publish interval are configurable to balance visibility vs log noise.

---

## Milestone 4 — Warm Standby & Parent Monitoring (implemented + hardware-validated)

Milestone 4 deliverable note:

- [`docs/milestone-4-warm-standby.md`](docs/milestone-4-warm-standby.md)

Milestone 4 outcome:

- Active parent monitoring via OpenThread parent RSSI/info APIs.
- Warm standby candidate maintenance via neighbor-table scanning + hysteresis.
- Metrics (log-based) for active parent quality, standby quality, and freshness.

Example YAML:

- [`examples/milestone-4-warm-standby.yaml`](examples/milestone-4-warm-standby.yaml)

## Milestone 5 — Deterministic Preferred-Failover Outcome Handling (implemented + hardware-validated)

Milestone 5 deliverable note:

- [`docs/milestone-5-handoff.md`](docs/milestone-5-handoff.md)

Milestone 5 outcome:

- Preferred reattach success/miss/timeout are classified deterministically.
- Miss/timeout paths force generic fallback with explicit logs and counters.
- Hardware validation captured success, miss, and timeout behavior on ESP32-C6 boards.

## Milestone 6 — ESPHome Integration + Observability (implemented)

Milestone 6 outcome:

- Optional ESPHome diagnostic entities expose active parent ID, standby parent ID, failover count, degraded state, and last failover reason.
- Configurable runtime debug hooks and diagnostics publish interval make log output easier to tune.
- Milestone 6 example configuration added for compile-time validation and easier adoption.
- ESP32-C6 spot-validation confirmed configured diagnostics cadence after excluding volatile age/freshness fields from diagnostics change-detection.

Example YAML:

- [`examples/milestone-6-observability.yaml`](examples/milestone-6-observability.yaml)

---

## Roadmap

- **Milestone 5:** Deterministic preferred failover outcome handling + fallback policy (**implemented and hardware-validated**)
- **Milestone 6:** ESPHome integration + observability (**implemented; compile-checked + hardware spot-validated cadence**)
- **Milestone 7:** ESP32-C6 hardware validation
- **Milestone 8:** Optimization + robustness
- **Milestone 9:** Documentation + release
