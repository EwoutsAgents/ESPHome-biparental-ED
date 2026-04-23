# ESPHome-biparental-ED

ESPHome external component for ESP32-C6 to implement **active-passive bi-parental behavior** for a Thread End Device (ED):

- one **active parent** used for all normal traffic,
- one **warm standby parent candidate** tracked and refreshed in the background,
- accelerated reattachment when active-parent quality becomes unacceptable.

This project intentionally stays aligned with Thread/OpenThread attachment behavior and does **not** assume direct failover from child context alone.

---

## Milestone 1 — Problem Definition & Architecture Freeze (started)

### 1) Scope freeze

This design is explicitly **active-passive**:

- At any moment, the child has exactly one attached parent.
- The standby is only a ranked, refreshed candidate (not a second active parent).
- Failover means performing a fresh (but accelerated/preferred) attachment attempt, compatible with Thread procedures.

### 2) In-scope / out-of-scope boundaries

**In scope (Milestone 1):**

- Behavior definitions and control logic.
- State machine and failure policy.
- Mapping logic to Thread primitives.

**Out of scope (Milestone 1):**

- Forking or patching OpenThread internals.
- Non-compliant shortcuts that bypass normal Parent Request/Child ID flows.
- Performance tuning and hardware benchmarks.

### 3) Architecture definition (frozen v0.1)

The external component is treated as an overlay controller around the Thread stack:

1. **Parent Health Evaluator**
   - consumes link/control-plane signals for current parent,
   - classifies parent as healthy/degraded/failed.

2. **Standby Candidate Tracker**
   - maintains best known alternate parent candidate,
   - refreshes candidate knowledge periodically,
   - enforces freshness window and replacement policy.

3. **Failover Orchestrator**
   - triggers reattachment when policy thresholds are crossed,
   - biases attach behavior toward preferred standby where possible,
   - applies hysteresis and hold-down timers.

4. **Diagnostics Surface**
   - exposes active parent identity, standby identity, state, reason codes, counters.

### 4) State machine definition (frozen v0.1)

```text
[BOOTSTRAP]
  -> ATTACHING_INITIAL

ATTACHING_INITIAL
  - success -> ATTACHED_ACTIVE_NO_STANDBY
  - timeout/retry -> ATTACHING_INITIAL

ATTACHED_ACTIVE_NO_STANDBY
  - candidate discovered -> ATTACHED_ACTIVE_STANDBY_WARM
  - active hard failure -> FAILOVER_TRIGGERED

ATTACHED_ACTIVE_STANDBY_WARM
  - active healthy -> stay
  - standby stale/lost -> ATTACHED_ACTIVE_NO_STANDBY
  - active degraded (sustained) -> FAILOVER_ELIGIBLE
  - active hard failure -> FAILOVER_TRIGGERED

FAILOVER_ELIGIBLE
  - degradation clears (within hysteresis) -> ATTACHED_ACTIVE_STANDBY_WARM
  - hold-down elapsed + degrade persists -> FAILOVER_TRIGGERED

FAILOVER_TRIGGERED
  -> REATTACHING_PREFERRED

REATTACHING_PREFERRED
  - attach to preferred standby succeeds -> POST_FAILOVER_STABILIZE
  - preferred fails -> REATTACHING_GENERIC

REATTACHING_GENERIC
  - attach succeeds -> POST_FAILOVER_STABILIZE
  - attach fails -> retry/backoff

POST_FAILOVER_STABILIZE
  - new active stable -> ATTACHED_ACTIVE_NO_STANDBY
```

### 5) Failure detection policy (frozen v0.1)

Failure classification uses three channels and explicit persistence windows:

1. **Hard failure**
   - no successful parent interaction for `hard_failure_timeout`.
   - immediate transition toward failover path.

2. **Link degradation**
   - poor RSSI/LQI and/or packet delivery quality below threshold.
   - must persist for `degrade_persist_time` before failover eligibility.

3. **Control-plane instability**
   - repeated attach/supervision anomalies, excessive short detach/reattach churn.
   - contributes to degradation score; can escalate to failover if sustained.

### 6) Decision policy and anti-flap controls

- Failover requires either:
  - hard failure, or
  - sustained degradation past threshold and hysteresis window.
- After failover, a **hold-down** interval prevents immediate ping-pong.
- Standby replacement is conservative: candidate must beat incumbent by margin and freshness.

### 7) Mapping to Thread primitives

Expected flow remains compatible with standard attach behavior:

- discovery/selection phase informed by Parent Request/Parent Response behavior,
- attachment via Child ID Request/Child ID Response,
- normal child supervision retained,
- no assumption that child can directly switch active parent without reattachment procedure.

### 8) Milestone 1 deliverables status

- [x] Architecture description
- [x] Parent selection and failover state machine
- [x] Failure detection logic definition

---

## Milestone 2 — Protocol-Level Feasibility Validation (completed)

Milestone 2 deliverable note:

- [`docs/milestone-2-protocol-feasibility.md`](docs/milestone-2-protocol-feasibility.md)

Milestone 2 outcome:

- Confirms feasibility of active-passive warm-standby behavior as an **external overlay**.
- Freezes compliance boundaries for what can be implemented without stack modifications.
- Explicitly separates compliant behavior from cases that would require OpenThread internal changes.

---

## Roadmap

- **Milestone 3:** ESPHome external component design + compileable scaffold
- **Milestone 4:** Warm standby implementation
- **Milestone 5:** Failover control + accelerated reattachment
- **Milestone 6:** ESPHome integration + observability
- **Milestone 7:** ESP32-C6 hardware validation
- **Milestone 8:** Optimization + robustness
- **Milestone 9:** Documentation + release
