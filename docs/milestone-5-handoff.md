# Milestone 5 — Deterministic Preferred-Failover Outcome Handling

Status: **implemented in code** (hardware validation pending in this session)

## Objective

Make preferred failover deterministic by classifying preferred reattach outcomes explicitly and enforcing a fixed policy for each outcome.

## Deterministic policy implemented

During `REATTACHING_PREFERRED`, the controller now distinguishes three outcomes:

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

Additional deterministic edge handling:

- If child attach occurs but parent identity cannot be resolved before timeout, the outcome is treated as **miss** and falls back to generic reattach.

## Key behavior guarantees

- `preferred_success_count` is incremented **only** on exact preferred-target match.
- Attach-to-non-target is no longer treated as preferred success.
- Miss/timeout always follow a deterministic fallback path to generic reattach.

## Diagnostics and logging added

### Snapshot diagnostics (periodic + on change)

Diagnostics now include, in addition to existing counters/state:

- `preferred_target_rloc16`
- `preferred_attached_parent_rloc16`
- `preferred_outcome` (`none|success|miss|timeout`)
- `preferred_result_state` (state after preferred decision)

### Explicit event logs

On each preferred outcome event:

- `Preferred outcome=<success|miss|timeout> target=0x.... attached=0x.... -> result_state=<...>`

On miss/timeout fallback action:

- `Preferred outcome=<miss|timeout> target=0x.... attached=0x.... -> action=generic_reattach`

These lines provide single-capture proof of preferred target, attached parent at decision time, outcome, and resulting transition/action.

## Files changed for Milestone 5

- `components/biparental_ed/failover_controller.h`
- `components/biparental_ed/failover_controller.cpp`
- `components/biparental_ed/biparental_ed.h`
- `components/biparental_ed/biparental_ed.cpp`
- `components/biparental_ed/diagnostics_publisher.h`
- `components/biparental_ed/diagnostics_publisher.cpp`

## Validation gates

### Build gate

Requested command:

- `esphome compile examples/milestone-4-warm-standby.yaml`

Current session result:

- blocked: `esphome` CLI/module is not installed in this runtime (`esphome: command not found`, `python3 -m esphome: No module named esphome`).

### Hardware gate

Requested evidence:

- preferred success,
- preferred miss,
- preferred timeout with deterministic fallback,
- matching counters/state transitions.

Current session result:

- blocked: no direct hardware disruption/serial capture was executed from this runtime.

### Controller policy gate (host-side deterministic check)

Executed a host-side C++ harness against `FailoverController` to verify outcome classification logic.

Observed key lines:

- `SUCCESS: ... outcome=1 result_state=8 a/s/m=1/1/0 failovers=1`
- `MISS: ... reason=2 ... attached=0x3333 outcome=2 result_state=7 a/s/m=1/0/1 failovers=0`
- `TIMEOUT: ... reason=3 ... attached=0xffff outcome=3 result_state=7 a/s/m=1/0/1 failovers=0`

Interpretation:

- `outcome=1` -> success only on target match.
- `outcome=2` -> miss on non-target attach, deterministic fallback to generic (`result_state=7`).
- `outcome=3` -> timeout with deterministic fallback to generic (`result_state=7`).

## Hardware evidence checklist (next run)

Capture at least one instance of each:

1. Success path
   - `Preferred outcome=success ... -> result_state=8`
   - diagnostics showing `preferred(a/s/m)` success increment.

2. Miss path
   - `Preferred outcome=miss ... -> result_state=7`
   - `Preferred outcome=miss ... -> action=generic_reattach`
   - diagnostics showing `preferred_miss_count` increment.

3. Timeout path
   - `Preferred outcome=timeout ... -> result_state=7`
   - `Preferred outcome=timeout ... -> action=generic_reattach`
   - diagnostics showing `preferred_miss_count` increment.

4. Counter consistency
   - `preferred_success_count` increments only on target match.
   - `preferred_miss_count` increments on miss/timeout.
