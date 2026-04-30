# Milestone 5 — Best-Effort Preferred-Parent Search Outcome Handling

Status: **implemented and hardware-validated**

## Objective

Implement explicit best-effort preferred-parent-search outcome handling:

- keep a preferred standby RLOC16 as policy intent,
- request OpenThread better-parent search as a best-effort action,
- classify result as success/miss/timeout,
- enforce fixed generic fallback policy for miss/timeout.

## Outcome policy implemented

During `REATTACHING_PREFERRED`, the controller distinguishes three outcomes:

1. **Preferred success**
   - Condition: attached as child **and** attached parent RLOC16 equals requested preferred target RLOC16.
   - Transition: `POST_FAILOVER_STABILIZE`.
   - Counters: `preferred_success_count++`, `failover_count++`.

2. **Preferred miss**
   - Condition: attached as child, attached parent RLOC16 is known, and attached parent != preferred target.
   - Transition/action: immediate transition to `REATTACHING_GENERIC` + `TRIGGER_GENERIC_REATTACH`.
   - Counters: `preferred_miss_count++`.

3. **Preferred timeout**
   - Condition: no child attach before `preferred_reattach_timeout` expires.
   - Transition/action: transition to `REATTACHING_GENERIC` + `TRIGGER_GENERIC_REATTACH`.
   - Counters: `preferred_miss_count++`.

Additional edge handling:

- If child attach occurs but parent identity cannot be resolved before timeout, the outcome is treated as **miss** and falls back to generic reattach.

## Key behavior guarantees

- `preferred_success_count` increments **only** on exact preferred-target match.
- Attach-to-non-target is treated as miss.
- Miss/timeout always follow deterministic fallback to generic reattach.

## Diagnostics and logging

### Snapshot diagnostics (periodic + on change)

Diagnostics include:

- `preferred_target_rloc16`
- `preferred_attached_parent_rloc16`
- `preferred_outcome` (`none|success|miss|timeout`)
- `preferred_result_state` (state after preferred decision)

### Explicit event logs

On each preferred outcome event:

- `Targeted standby outcome=<success|miss|timeout> target=0x.... attached=0x.... -> result_state=<...>`

On miss/timeout fallback action:

- `Targeted standby outcome=<miss|timeout> target=0x.... attached=0x.... -> action=generic_reattach`

These lines provide single-capture proof of preferred intent target, attached parent at decision time, outcome, and resulting transition/action.

## Files changed for Milestone 5

- `components/biparental_ed/failover_controller.h`
- `components/biparental_ed/failover_controller.cpp`
- `components/biparental_ed/biparental_ed.h`
- `components/biparental_ed/biparental_ed.cpp`
- `components/biparental_ed/diagnostics_publisher.h`
- `components/biparental_ed/diagnostics_publisher.cpp`

## Validation gates

### Build gate

Executed in-session with ESPHome v2026.4.1:

- `esphome compile examples/hwtest-ed.yaml` ✅
- `esphome upload examples/hwtest-router.yaml --device /dev/ttyACM0` ✅
- `esphome upload examples/hwtest-router.yaml --device /dev/ttyACM3` ✅

### Hardware gate

Direct serial capture completed on physical boards (`/dev/ttyACM0`, `/dev/ttyACM1`, `/dev/ttyACM3`, `/dev/ttyACM4`).

Evidence log: `/tmp/m5-hw-ed.log`

Observed preferred outcomes in hardware logs:

- **Miss**
  - `[12:58:56.069] Targeted standby outcome=miss target=0x5400 attached=0xc800 -> result_state=7`
  - `[12:58:56.080] Targeted standby outcome=miss ... -> action=generic_reattach`

- **Success**
  - `[12:59:17.064] Targeted standby outcome=success target=0x5400 attached=0x5400 -> result_state=8`

- **Timeout**
  - `[13:01:10.167] Targeted standby outcome=timeout target=0xc800 attached=0xffff -> result_state=7`
  - `[13:01:10.178] Targeted standby outcome=timeout ... -> action=generic_reattach`

Counter/state consistency observed in diagnostics around those events:

- success increments `preferred_success_count` only on target match,
- miss/timeout increment miss slot and transition through generic fallback (`result_state=7`).

### Controller policy gate (host-side deterministic check)

Executed a host-side C++ harness against `FailoverController` to verify outcome classification logic.

Observed key lines:

- `SUCCESS: ... outcome=1 result_state=8 a/s/m=1/1/0 failovers=1`
- `MISS: ... reason=2 ... attached=0x3333 outcome=2 result_state=7 a/s/m=1/0/1 failovers=0`
- `TIMEOUT: ... reason=3 ... attached=0xffff outcome=3 result_state=7 a/s/m=1/0/1 failovers=0`

## Hardware evidence checklist

All required paths captured in hardware:

1. ✅ Success path
2. ✅ Miss path with deterministic generic fallback
3. ✅ Timeout path with deterministic generic fallback
4. ✅ Counter consistency (success only on target match; miss slot used for miss/timeout)
