# Milestone 5 — Handoff for a New Agent Context

Status: **ready to start**

## What is already done (carry-over state)

- Milestone 4 monitoring + standby maintenance is implemented and hardware-validated.
- Preferred failover request path exists in OT adapter (`request_failover_to_preferred`).
- Continuity fallback keeps previous active parent as standby on parent switch.
- Preferred observability counters already exist:
  - `preferred_attempt_count`
  - `preferred_success_count`
  - `preferred_miss_count`

Recent commits that matter:
- `0306aeb` — best-effort preferred failover OT path.
- `aa77706` — standby continuity fallback on parent switch.
- `4a57318` — preferred failover observability counters.

## Why Milestone 5 is still needed

Current preferred failover is **best-effort request issuance**, not deterministic target control.
The stack can reattach to a non-target parent and still be treated as "success" by current state logic.

## Milestone 5 objective

Implement deterministic preferred-failover outcome handling:

1. Distinguish:
   - preferred success (attached to requested standby parent),
   - preferred miss (attached, but to another parent),
   - preferred timeout (no child attach within timeout).
2. Enforce explicit policy on miss/timeout (fallback/retry behavior).
3. Keep diagnostics explicit so hardware runs prove behavior.

## Recommended implementation plan

### 1) Extend failover state evaluation inputs

Current `FailoverController::evaluate(...)` does not know:
- current active parent RLOC,
- preferred target RLOC,
- whether current parent equals preferred target.

Add this context so preferred outcome is computed by policy, not inferred from attach-only state.

### 2) Add explicit preferred outcome policy

Recommended default policy:
- While in `REATTACHING_PREFERRED`:
  - if attached and parent == preferred target: mark preferred success.
  - if attached and parent != preferred target: mark preferred miss and transition to generic path.
  - if timeout before success: mark preferred miss/timeout and transition to generic path.

(If you prefer a softer policy, gate miss->generic behind a small retry budget.)

### 3) Keep/expand observability

Counters already exist; add any missing context needed for hardware proof, e.g.:
- last preferred target RLOC,
- last attached parent RLOC at preferred decision point,
- last preferred outcome enum.

### 4) Preserve standby continuity after miss

Ensure candidate lifecycle does not immediately lose useful standby context after a miss + reattach.
(Continuity fallback is already present; verify interaction with your new preferred-miss transitions.)

## Files most likely to change

Core logic:
- `components/biparental_ed/failover_controller.h`
- `components/biparental_ed/failover_controller.cpp`
- `components/biparental_ed/biparental_ed.cpp`

Diagnostics:
- `components/biparental_ed/diagnostics_publisher.h`
- `components/biparental_ed/diagnostics_publisher.cpp`

Optional adapter hooks (only if policy needs them):
- `components/biparental_ed/ot_platform_adapter.h`
- `components/biparental_ed/ot_platform_adapter.cpp`

## Validation checklist (Milestone 5)

Build gate:
- `esphome compile examples/milestone-4-warm-standby.yaml`

Hardware gate (recommended):
1. Flash ED + two routers.
2. Capture ED logs during forced router disruption.
3. Verify logs show all three classes over runs:
   - preferred success,
   - preferred miss,
   - timeout/fallback behavior.
4. Verify counters and state transitions are consistent with policy.

## Suggested done criteria for Milestone 5

- [ ] Preferred success is only counted when attached parent equals requested target.
- [ ] Preferred miss/timeout are explicitly detected and logged.
- [ ] Miss/timeout policy is deterministic and documented.
- [ ] Diagnostics can prove behavior from a single log capture.
- [ ] Build compiles cleanly and hardware run reproduces expected transitions.
