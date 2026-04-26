# Complete Documentation — biparental_ed

## Purpose

`biparental_ed` is an ESPHome external component for ESP32-C6 + OpenThread that adds an **active/passive parent failover policy** on top of the normal Thread child attach flow.

It keeps:

- one **active parent** for live traffic,
- one **warm standby candidate** in controller state,
- a policy engine that can trigger **preferred** or **generic** reattachment when the active parent becomes unhealthy.

The implementation is intentionally an **overlay**, not a Thread stack fork.

---

## 1. Architecture

### Runtime layers

1. **ESPHome component shell** (`BiparentalEDComponent`)
   - extends `PollingComponent`
   - owns configuration, update loop, diagnostics publication, and OpenThread adapter selection

2. **Parent health monitor** (`ParentHealthMonitor`)
   - consumes parent metrics
   - classifies state as `HEALTHY`, `DEGRADED`, or `FAILED`
   - reports fail reason as `NONE`, `HARD_TIMEOUT`, `LINK_DEGRADED`, or `CONTROL_PLANE`

3. **Candidate manager** (`CandidateManager`)
   - tracks current active parent RLOC16
   - maintains one standby candidate with score, RSSI, link margin, and freshness timestamp
   - applies staleness and replacement hysteresis

4. **Failover controller** (`FailoverController`)
   - runs the state machine
   - decides when to trigger preferred or generic reattach
   - records failover counts and preferred outcome classification

5. **Diagnostics publisher** (`DiagnosticsPublisher`)
   - emits compact snapshots on change or configured interval
   - optionally publishes ESPHome diagnostic entities

6. **OpenThread platform adapter** (`EspHomeOpenThreadPlatformAdapter`)
   - reads current parent metrics
   - scans router neighbors
   - requests parent search, preferred failover, or generic reattach

### Main data flow

```text
OpenThread parent/neighbor state
  -> OpenThreadPlatformAdapter
  -> ParentHealthMonitor + CandidateManager
  -> FailoverController
  -> failover action request
  -> OpenThread detach/reattach path
  -> DiagnosticsPublisher + optional ESPHome entities
```

### State machine

Implemented states:

- `BOOTSTRAP`
- `ATTACHING_INITIAL`
- `ATTACHED_ACTIVE_NO_STANDBY`
- `ATTACHED_ACTIVE_STANDBY_WARM`
- `FAILOVER_ELIGIBLE`
- `FAILOVER_TRIGGERED`
- `REATTACHING_PREFERRED`
- `REATTACHING_GENERIC`
- `POST_FAILOVER_STABILIZE`

Operationally:

- attach normally
- discover/refresh a standby candidate
- monitor active parent health
- on hard failure or sustained degradation, trigger failover
- prefer the standby if available
- fall back to generic reattach if preferred outcome misses or times out

---

## 2. Design decisions

### External overlay, not stack modification

The component does not patch OpenThread internals. It only uses public attach/search/detach APIs and normal Thread role transitions.

Why this matters:

- keeps implementation aligned with standard Thread behavior
- avoids maintaining an OpenThread fork
- limits how strongly a specific standby parent can be forced

### Single standby candidate, not multi-parent attachment

The candidate manager stores exactly one standby candidate. This matches the project definition from Milestones 1 and 2: one active parent plus one ranked alternate.

### Conservative candidate replacement

Standby replacement requires either:

- no current standby,
- a stale standby, or
- a new score beating the existing standby by `standby_replace_hysteresis`

The score is computed cheaply as:

```text
score = link_margin + (rssi / 2)
```

### Hard failure and sustained degradation are separated

`ParentHealthMonitor` treats these differently:

- invalid metrics, supervision failure, or parent RX timeout => `FAILED`
- low RSSI or control-plane instability that persists long enough => `DEGRADED`

This keeps the controller from reacting to short-lived dips.

### Preferred failover is deterministic in outcome handling

Milestone 5 made the policy deterministic after a preferred failover request:

- attached to requested standby => `SUCCESS`
- attached to a different parent => `MISS`
- no attach before timeout => `TIMEOUT`

Miss and timeout both force a generic fallback path.

### Neighbor-table discovery is the current standby source

The current implementation refreshes standby candidates primarily from router-neighbor scans plus continuity fallback when the active parent changes.

### Entity publication is optional

The component can stay log-only, or it can publish:

- `active_parent_id`
- `standby_parent_id`
- `failover_count`
- `degraded_state`
- `last_failover_reason`

---

## 3. Limitations vs standard Thread behavior

These are the important boundaries documented by the repo and visible in code.

### No dual active parents

This is **not** active/active Thread behavior. The device is still attached to only one parent at a time.

### Parent switching still uses normal attach/reattach

There is no direct parent swap primitive here. The component triggers detach/reattach and lets OpenThread execute the normal attach path.

### Preferred parent selection is best-effort

`request_failover_to_preferred()` records a target and may start `otThreadSearchForBetterParent()`, detach, and reattach. But the final parent choice remains OpenThread-controlled.

That means:

- a preferred target can succeed,
- or OpenThread can attach to a different acceptable parent,
- or the attempt can time out.

### Parent search refresh may itself move the device

`parent_search_refresh_interval` is explicitly opt-in because the code warns that `otThreadSearchForBetterParent()` may cause an automatic parent switch.

### Parent-response callback is not yet runtime-wired

The YAML option `enable_parent_response_callback` exists, and setup logs that callback integration was requested, but the component `loop()` still describes this as a future integration point. Current standby discovery is therefore not dependent on a live registered parent-response callback.

### Control-plane degradation is only partially realized

The health monitor supports `control_plane_error_threshold`, but the OpenThread adapter currently sets `control_plane_error_count` to `0`. So the config key exists, but real control-plane error accumulation is not yet implemented through the current adapter.

### Metrics are limited by public OpenThread visibility

Parent age is approximated from neighbor age, and last parent RX time is derived from that age. This is practical, but not equivalent to deep internal stack telemetry.

### Not a generic Thread-device abstraction

The project is specifically validated around ESPHome + ESP32-C6 + OpenThread integration patterns used in this repo.

---

## 4. Current configuration surface

Supported `biparental_ed:` keys from the Python config layer:

### Health policy

- `degraded_rssi_threshold`
- `hard_failure_timeout`
- `degrade_persist_time`
- `control_plane_error_threshold`

### Failover policy

- `hold_down_time`
- `failover_eligible_delay`
- `post_failover_stabilize_time`
- `preferred_reattach_timeout`
- `enable_parent_response_callback`

### Standby maintenance

- `standby_refresh_interval`
- `standby_replace_hysteresis`
- `neighbor_scan_interval`
- `neighbor_max_age`
- `parent_search_refresh_interval`

### Observability

- `runtime_debug_enabled`
- `runtime_debug_interval`
- `verbose_diagnostics`
- `diagnostics_publish_interval`
- optional diagnostic entities listed above

---

## 5. Setup instructions

## Hardware used by this repo

The existing examples and milestone validation target **ESP32-C6** boards using **ESP-IDF** and ESPHome's `openthread:` component.

For realistic failover tests you need at least:

- 1 ESP32-C6 configured as the ED running `biparental_ed`
- 2 or more ESP32-C6 boards acting as Thread routers/FTDs

## Software prerequisites

- ESPHome with OpenThread support
- serial access to the boards
- this repository available locally or via git `external_components`

## Install the external component

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/EwoutsAgents/ESPHome-biparental-ED
      ref: main
      path: components
    components: [biparental_ed]
```

For local development, the repo examples use:

```yaml
external_components:
  - source:
      type: local
      path: ../components
    components: [biparental_ed]
```

## Minimum device requirements

Use:

- `esp32:`
  - `board: esp32-c6-devkitc-1`
  - `framework.type: esp-idf`
- `network.enable_ipv6: true`
- `mdns:`
- `openthread:`

The ED example in this repo uses `device_type: MTD`. Router examples use `device_type: FTD`.

## Basic bring-up workflow

1. Prepare one YAML for the ED and one or more YAMLs for routers.
2. Make sure all nodes share the same Thread dataset/network settings.
3. Compile the router firmware and flash router boards.
4. Compile the ED firmware and flash the ED board.
5. Start logs on the ED and confirm:
   - OpenThread is detected
   - parent metrics become valid
   - standby discovery eventually reports a standby candidate
6. Disrupt the active parent path to observe preferred/generic failover behavior.

## Compile / flash / log workflow

Typical commands used in milestone validation:

```bash
esphome compile examples/hwtest-ed.yaml
esphome upload examples/hwtest-router.yaml --device /dev/ttyACM0
esphome logs examples/hwtest-ed.yaml --device /dev/ttyACM1
```

Milestone 7 also documents a direct `esptool` flash path used when `esphome upload` was unreliable on `/dev/ttyACM0`.

If upload is unstable:

- retry with a known-good serial port
- reduce moving parts during testing
- consider direct `esptool` flashing as used in `docs/milestone-7-hardware-validation.md`

---

## 6. Example ESPHome configurations

## Example A — normal ED profile

This is a safe, current configuration based on `examples/hwtest-ed.yaml` plus the component keys used across milestones.

```yaml
substitutions:
  devicename: biparental-ed
  friendly: "Biparental ED"

esphome:
  name: ${devicename}
  friendly_name: ${friendly}

esp32:
  board: esp32-c6-devkitc-1
  framework:
    type: esp-idf
    log_level: INFO

logger:
  level: DEBUG
  hardware_uart: UART0
  deassert_rts_dtr: true

network:
  enable_ipv6: true

mdns:
  id: mdns_comp

openthread:
  mdns_id: mdns_comp
  device_type: MTD
  force_dataset: true
  channel: 15
  pan_id: 0x1234
  ext_pan_id: 0x1111111122222222
  network_name: biparental-test
  network_key: 0x10112233445566778899AABBCCDDEEFF
  pskc: 0x112233445566778899AABBCCDDEEFF00
  mesh_local_prefix: "fd11:22:33:44::/64"

external_components:
  - source:
      type: local
      path: ../components
    components: [biparental_ed]

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

## Example B — timeout-testing ED profile

This follows the timing direction documented in Milestone 7 for forcing faster timeout behavior during hardware validation.

```yaml
esphome:
  name: biparental-ed-timeout
  friendly_name: Biparental ED Timeout Test

esp32:
  board: esp32-c6-devkitc-1
  framework:
    type: esp-idf

logger:
  level: DEBUG

network:
  enable_ipv6: true

mdns:
  id: mdns_comp

openthread:
  mdns_id: mdns_comp
  device_type: MTD
  force_dataset: true
  channel: 15
  pan_id: 0x1234
  ext_pan_id: 0x1111111122222222
  network_name: biparental-test
  network_key: 0x10112233445566778899AABBCCDDEEFF
  pskc: 0x112233445566778899AABBCCDDEEFF00
  mesh_local_prefix: "fd11:22:33:44::/64"

external_components:
  - source:
      type: local
      path: ../components
    components: [biparental_ed]

biparental_ed:
  id: biparental_controller
  update_interval: 200ms
  degraded_rssi_threshold: -72
  hard_failure_timeout: 15s
  degrade_persist_time: 10s
  hold_down_time: 30s
  standby_refresh_interval: 60s
  failover_eligible_delay: 10s
  post_failover_stabilize_time: 10s
  preferred_reattach_timeout: 300ms
  enable_parent_response_callback: true
  control_plane_error_threshold: 3
  standby_replace_hysteresis: 5
  neighbor_scan_interval: 10s
  neighbor_max_age: 60s
  parent_search_refresh_interval: 0s
  runtime_debug_enabled: true
  runtime_debug_interval: 500ms
  verbose_diagnostics: true
  diagnostics_publish_interval: 1s

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

---

## 7. What to watch in logs

Useful lines documented by the implementation and milestone logs include:

- `OpenThread detected; enabling runtime adapter`
- `runtime: got_metrics=...`
- `Triggered OpenThread better-parent search refresh`
- `Requesting preferred failover (target=0x....)`
- `Preferred outcome=success|miss|timeout target=... attached=... -> result_state=...`
- `Preferred outcome=miss|timeout ... -> action=generic_reattach`

Diagnostic snapshots include:

- failover state
- health state and reason
- active parent RLOC/RSSI/link margin/age
- standby RLOC/score/RSSI/link margin/freshness
- failover counters
- preferred attempt/success/miss counters

---

## 8. Test results summarized from milestone docs

## Milestone 4

Documented as implemented and hardware-validated.

Observed during disruption tests:

- standby acquisition became valid
- preferred failover path was issued and accepted
- reattach transitions were observed with failover counter progression

Evidence reference in docs: `/tmp/biparental-ed-validation-8.log`

## Milestone 5

Validation documented three preferred outcomes on hardware:

- **miss**
  - `Preferred outcome=miss target=0x5400 attached=0xc800 -> result_state=7`
- **success**
  - `Preferred outcome=success target=0x5400 attached=0x5400 -> result_state=8`
- **timeout**
  - `Preferred outcome=timeout target=0xc800 attached=0xffff -> result_state=7`

Milestone 5 also reports:

- compile success for `examples/hwtest-ed.yaml`
- successful router uploads to `/dev/ttyACM0` and `/dev/ttyACM3`
- host-side controller checks confirming deterministic success/miss/timeout classification

## Milestone 6

Documented as implemented, with:

- compile-time validation for the milestone example
- ESP32-C6 spot-validation of runtime debug and diagnostics cadence
- fix for overly chatty diagnostics caused by volatile age/freshness fields

## Milestone 7

Closeout passed with fresh timeout-specific evidence:

- `Preferred outcome=timeout ...`
- `entity.last_failover_reason=preferred_timeout`

Milestone 7 also records that direct `esptool` flashing was used successfully when `esphome upload` was intermittently unreliable.

---

## 9. Known issues and practical caveats

### Preferred failover is not strict parent pinning

The component can prefer a target, but OpenThread may still attach elsewhere. This is expected and already handled as a deterministic `miss`.

### Serial/upload instability was observed during hardware work

Milestone 7 explicitly notes intermittent `esphome upload` connection failures on `/dev/ttyACM0`, with direct `esptool` flashing used as the stable workaround in that validation pass.

### Parent-response callback integration is incomplete

The option exists, but the code still treats callback registration as future wiring. Do not assume current standby tracking depends on live parent-response callback registration.

### Control-plane error counting is not fully implemented

The config knob is present, but adapter metrics currently report `control_plane_error_count = 0`, so link/RX-timeout signals carry most of the practical decision weight today.

### Neighbor-based standby quality is inherently approximate

Standby scoring is based on neighbor-derived RSSI/link margin snapshots and freshness windows, not a fully attached standby relationship.

### Parent search refresh can be disruptive

Because OpenThread better-parent search may switch parents automatically, `parent_search_refresh_interval` should stay disabled unless you explicitly want that behavior.

---

## 10. File map

Primary implementation files:

- `components/biparental_ed/__init__.py`
- `components/biparental_ed/biparental_ed.h`
- `components/biparental_ed/biparental_ed.cpp`
- `components/biparental_ed/parent_health_monitor.*`
- `components/biparental_ed/candidate_manager.*`
- `components/biparental_ed/failover_controller.*`
- `components/biparental_ed/diagnostics_publisher.*`
- `components/biparental_ed/ot_platform_adapter.*`

Examples and milestone references:

- `examples/hwtest-ed.yaml`
- `examples/hwtest-router.yaml`
- `examples/milestone-4-warm-standby.yaml`
- `examples/milestone-6-observability.yaml`
- `docs/milestone-1-architecture-freeze.md`
- `docs/milestone-2-protocol-feasibility.md`
- `docs/milestone-3-external-component-design.md`
- `docs/milestone-4-warm-standby.md`
- `docs/milestone-5-handoff.md`
- `docs/milestone-6-observability.md`
- `docs/milestone-7-hardware-validation.md`

---

## 11. Milestone history and roadmap

### Milestone 1 — Problem Definition & Architecture Freeze (completed)

Deliverable:

- [`docs/milestone-1-architecture-freeze.md`](docs/milestone-1-architecture-freeze.md)

Outcome:

- Architecture and behavior definition frozen as active-passive overlay design.
- Parent selection and failover state machine defined.
- Failure detection and anti-flap policy defined.

### Milestone 2 — Protocol-Level Feasibility Validation (completed)

Deliverable:

- [`docs/milestone-2-protocol-feasibility.md`](docs/milestone-2-protocol-feasibility.md)

Outcome:

- Confirms feasibility of active-passive warm-standby behavior as an external overlay.
- Freezes compliance boundaries for what can be implemented without stack modifications.
- Separates compliant behavior from cases requiring OpenThread internal changes.

### Milestone 3 — ESPHome External Component Design (completed)

Deliverable:

- [`docs/milestone-3-external-component-design.md`](docs/milestone-3-external-component-design.md)

Outcome:

- External component scaffold added under `components/biparental_ed/`.
- Core internal modules defined (`ParentHealthMonitor`, `CandidateManager`, `FailoverController`, `DiagnosticsPublisher`).
- Internal failover state machine scaffold implemented.
- YAML configuration schema and first example added.

### Milestone 4 — Warm Standby & Parent Monitoring (implemented + hardware-validated)

Deliverable:

- [`docs/milestone-4-warm-standby.md`](docs/milestone-4-warm-standby.md)

Outcome:

- Active parent monitoring via OpenThread parent RSSI/info APIs.
- Warm-standby candidate maintenance via neighbor-table scanning + hysteresis.
- Log-based metrics for active and standby quality/freshness.

Example YAML:

- [`examples/milestone-4-warm-standby.yaml`](examples/milestone-4-warm-standby.yaml)

### Milestone 5 — Deterministic Preferred-Failover Outcome Handling (implemented + hardware-validated)

Deliverable:

- [`docs/milestone-5-handoff.md`](docs/milestone-5-handoff.md)

Outcome:

- Preferred reattach success/miss/timeout classified deterministically.
- Miss/timeout force generic fallback with explicit logging/counters.
- Hardware validation captured success, miss, and timeout behavior on ESP32-C6 boards.

### Milestone 6 — ESPHome Integration + Observability (implemented)

Deliverable:

- [`docs/milestone-6-observability.md`](docs/milestone-6-observability.md)

Outcome:

- Optional diagnostic entities expose active/standby parent IDs, failover count, degraded state, and last failover reason.
- Runtime debug hooks and diagnostics publish interval are configurable.
- Milestone 6 example config was added and compile-validated.
- ESP32-C6 spot-validation confirmed diagnostics cadence after excluding volatile age/freshness from change detection.

Example YAML:

- [`examples/milestone-6-observability.yaml`](examples/milestone-6-observability.yaml)

### Milestone 7 — Hardware Validation Closeout (completed)

Deliverable:

- [`docs/milestone-7-hardware-validation.md`](docs/milestone-7-hardware-validation.md)

Outcome:

- Timeout closeout criteria captured on fresh hardware logs:
  - `Preferred outcome=timeout`
  - `entity.last_failover_reason=preferred_timeout`
