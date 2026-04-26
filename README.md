# ESPHome-biparental-ED

ESPHome external component for ESP32-C6 to implement **active-passive bi-parental behavior** for a Thread End Device (ED):

- one **active parent** used for all normal traffic,
- one **warm standby parent candidate** tracked and refreshed in the background,
- accelerated reattachment when active-parent quality becomes unacceptable.

This project intentionally stays aligned with Thread/OpenThread attachment behavior and does **not** assume direct failover from child context alone.

---

## Project status and milestones

Milestone history, outcomes, and roadmap were moved out of this README to keep the root page focused.

See:

- [`docs/complete-documentation.md#11-milestone-history-and-roadmap-moved-from-readme`](docs/complete-documentation.md#11-milestone-history-and-roadmap-moved-from-readme)

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

## Documentation

- Complete reference: [`docs/complete-documentation.md`](docs/complete-documentation.md)
- Milestone history + roadmap: [`docs/complete-documentation.md#11-milestone-history-and-roadmap-moved-from-readme`](docs/complete-documentation.md#11-milestone-history-and-roadmap-moved-from-readme)
- Detailed milestone notes: `docs/milestone-*.md`
