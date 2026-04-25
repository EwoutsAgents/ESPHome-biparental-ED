# Milestone 6 — ESPHome Integration + Observability

Status: **implemented**

## Objective

Expose the biparental controller's runtime state through optional ESPHome entities and make diagnostic logging tunable from YAML without breaking existing configurations.

## Implemented

### Optional ESPHome diagnostic entities

The external component now accepts optional YAML blocks for:

- `active_parent_id` (`text_sensor`)
- `standby_parent_id` (`text_sensor`)
- `failover_count` (`sensor`)
- `degraded_state` (`binary_sensor`)
- `last_failover_reason` (`text_sensor`)

When omitted, the component remains backward compatible and log-only.

### YAML-configurable debug hooks

The component now keeps all existing policy/timer knobs and adds:

- `runtime_debug_enabled`
- `runtime_debug_interval`
- `verbose_diagnostics`
- `diagnostics_publish_interval`

These allow tuning periodic runtime traces and diagnostics cadence separately.

### Diagnostics publication path

Entity publication is wired into the same diagnostics path that already emitted log snapshots, so logs and entity state stay aligned.

## Example

- [`examples/milestone-6-observability.yaml`](../examples/milestone-6-observability.yaml)

## Notes

- Compile-time validation passed for the milestone example.
- Hardware spot-validation on ESP32-C6 confirmed runtime debug + diagnostics cadence behavior.
- A cadence bug was found/fixed: diagnostics change-detection now ignores volatile age/freshness fields so `diagnostics_publish_interval` is respected in steady state.
- Full failover transition hardware validation remains in later hardware-focused milestones.
