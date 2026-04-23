# Milestone 3 — ESPHome External Component Design

Status: **completed (scaffold v0.1)**

## Goal

Define the software architecture of the ESPHome external component and provide a compileable scaffold.

## Deliverables

- [x] Repository structure
- [x] Component class definitions
- [x] YAML configuration interface
- [x] Initial compileable component scaffold

## Repository structure

```text
components/
  biparental_ed/
    __init__.py
    CMakeLists.txt
    biparental_ed.h
    biparental_ed.cpp
    parent_health_monitor.h
    parent_health_monitor.cpp
    candidate_manager.h
    candidate_manager.cpp
    failover_controller.h
    failover_controller.cpp
    diagnostics_publisher.h
    diagnostics_publisher.cpp
    ot_platform_adapter.h
    ot_platform_adapter.cpp
examples/
  milestone-3-scaffold.yaml
```

## ESPHome lifecycle integration

`BiparentalEDComponent` extends `PollingComponent` and implements:

- `setup()` for runtime configuration + module wiring
- `loop()` as callback integration point (for OpenThread callback registration in later milestones)
- `update()` as periodic state-machine evaluation tick
- `dump_config()` for runtime observability

## Internal modules

### 1) ParentHealthMonitor

Responsibilities:
- evaluate active parent health using RSSI/supervision/control-plane signals,
- classify health into `HEALTHY`, `DEGRADED`, `FAILED`,
- emit fail reason (`HARD_TIMEOUT`, `LINK_DEGRADED`, `CONTROL_PLANE`).

### 2) CandidateManager

Responsibilities:
- track active parent identity and warm standby candidate,
- ingest candidate data from parent-response observations,
- apply replace hysteresis and staleness policy.

### 3) FailoverController

Responsibilities:
- implement milestone-1 state machine states,
- transition across attach/degrade/failover phases,
- emit failover actions (`TRIGGER_PREFERRED_REATTACH`, `TRIGGER_GENERIC_REATTACH`),
- enforce hold-down and timeout policy.

### 4) DiagnosticsPublisher

Responsibilities:
- publish compact state snapshots,
- log only on meaningful change to reduce log noise.

## State machine implementation

Implemented in `failover_controller.*` with the following states:

- `BOOTSTRAP`
- `ATTACHING_INITIAL`
- `ATTACHED_ACTIVE_NO_STANDBY`
- `ATTACHED_ACTIVE_STANDBY_WARM`
- `FAILOVER_ELIGIBLE`
- `FAILOVER_TRIGGERED`
- `REATTACHING_PREFERRED`
- `REATTACHING_GENERIC`
- `POST_FAILOVER_STABILIZE`

## OpenThread integration boundary (Milestone 3)

`OpenThreadPlatformAdapter` defines the integration contract while keeping this scaffold buildable before full OpenThread wiring:

- `is_attached_as_child()`
- `read_parent_metrics(...)`
- `request_parent_search()`
- `request_failover_to_preferred(...)`
- `request_failover_generic()`

A `NoopOpenThreadPlatformAdapter` is provided as temporary fallback. Full API wiring is deferred to Milestones 4/5.

## YAML configuration interface

The component exposes:

- `degraded_rssi_threshold`
- `hard_failure_timeout`
- `degrade_persist_time`
- `hold_down_time`
- `standby_refresh_interval`
- `failover_eligible_delay`
- `post_failover_stabilize_time`
- `preferred_reattach_timeout`
- `enable_parent_response_callback`
- `control_plane_error_threshold`
- `standby_replace_hysteresis`
- `update_interval` (PollingComponent)

## Known limitations of this scaffold

1. OpenThread parent-response callback registration is not yet wired.
2. Reattach/failover requests are adapter stubs (policy and hooks are present; runtime OT calls are deferred).
3. Diagnostics are log-based in Milestone 3; ESPHome entity exposure is planned for Milestone 6.
