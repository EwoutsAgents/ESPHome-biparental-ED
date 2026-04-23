# ESPHome-biparental-ED
This GitHub repository implements an ESPHome external component for the ESP32-C6 that introduces active-passive bi-parental behavior for a Thread End Device. The device maintains one active parent alongside one warm standby parent candidate. The standby candidate is kept warm by periodically refreshing parent-search information while the device remains attached to the active parent. If the active parent fails or its link quality degrades, the device should perform accelerated reattachment toward the preferred standby parent using the freshest available candidate information. The design must not assume direct failover based solely on a Child ID Request; instead, failover should remain compatible, wherever possible, with standard Thread/OpenThread attachment procedures. After failover, the newly selected parent becomes the active parent, and the device must then identify and warm a new standby candidate.

Below is a clean continuation of your description using structured milestones, aligned with everything defined so far and suitable for both documentation and guiding implementation.

---

## Milestones

### Milestone 1 — Problem Definition & Architecture Freeze

**Goal:** Establish a precise and unambiguous definition of bi-parental behavior within Thread constraints.

**Scope:**

* Define the system as **active-passive** (not active-active).
* Formalize that:

  * only one parent is active at any time,
  * the standby parent is a **warm candidate**, not a co-parent.
* Define **parent failure conditions**, including:

  * hard failure (timeouts, lost communication),
  * degraded link (RSSI/LQI thresholds, packet loss),
  * control-plane instability.
* Map behavior to Thread primitives:

  * MLE Parent Request / Parent Response,
  * Child ID Request / Response,
  * child supervision and reattachment behavior.

**Deliverables:**

* Architecture description
* Parent selection and failover state machine
* Definition of failure detection logic

---

### Milestone 2 — Protocol-Level Feasibility Validation

**Goal:** Ensure the design is viable within Thread/OpenThread without requiring stack modification.

**Scope:**

* Validate constraints of the Thread protocol:

  * single-parent child model,
  * parent-scoped child identity,
  * routing via one parent only.
* Confirm feasibility of:

  * standby-parent awareness,
  * periodic parent-search refresh (“warm standby”),
  * accelerated reattachment toward a preferred parent.
* Explicitly identify boundaries:

  * what remains compliant,
  * what would require modifying OpenThread.

**Deliverables:**

* Technical note: “What must be hacked vs. what remains compliant”
* Defined strategy: **external component overlay (no stack fork)**

---

### Milestone 3 — ESPHome External Component Design

**Goal:** Define the software architecture of the external component.

**Scope:**

* Design component structure for ESPHome:

  * lifecycle integration (setup, loop, callbacks),
  * interaction with OpenThread APIs.
* Define internal modules:

  * ParentHealthMonitor
  * CandidateManager
  * FailoverController
  * DiagnosticsPublisher
* Specify the internal state machine implementation.
* Define configuration schema exposed via YAML.

**Deliverables:**

* Repository structure
* Component class definitions
* YAML configuration interface
* Initial compileable component scaffold

---

### Milestone 4 — Warm Standby & Parent Monitoring Implementation

**Goal:** Implement continuous monitoring and standby maintenance.

**Scope:**

* Implement:

  * active parent health monitoring,
  * periodic parent-search refresh,
  * candidate scoring and ranking,
  * standby replacement logic.
* Ensure:

  * hysteresis to prevent oscillation,
  * stability of standby selection,
  * minimal overhead during normal operation.

**Deliverables:**

* Functional warm-standby mechanism
* Metrics for:

  * active parent quality,
  * standby candidate quality,
  * freshness of standby data

---

### Milestone 5 — Failover Control & Accelerated Reattachment

**Goal:** Implement robust and deterministic failover behavior.

**Scope:**

* Detect failure conditions based on defined policies.
* Trigger accelerated reattachment toward the standby parent.
* Ensure compatibility with Thread attach procedures:

  * Parent Request / Response
  * Child ID Request / Response
* Implement:

  * failover hysteresis,
  * hold-down timers,
  * retry logic.

**Deliverables:**

* Working failover mechanism
* Verified transition between parents
* Failover event logging and diagnostics

---

### Milestone 6 — ESPHome Integration & Observability

**Goal:** Expose functionality and diagnostics to ESPHome users.

**Scope:**

* Provide YAML configuration options:

  * thresholds,
  * timers,
  * failover strategy parameters.
* Expose entities:

  * active parent ID
  * standby parent ID
  * failover count
  * degraded state
  * last failover reason
* Integrate logging and debugging hooks.

**Deliverables:**

* Fully usable ESPHome external component
* Example configurations
* Debug-friendly output

---

### Milestone 7 — Hardware Validation on ESP32-C6 Testbed

**Goal:** Validate behavior on real devices under realistic conditions.

**Scope:**

* Use 5-node setup:

  * 2 parent routers,
  * 1 end device under test,
  * supporting nodes (leader / traffic generator).
* Test scenarios:

  * initial attach,
  * standby discovery and warming,
  * parent degradation,
  * hard parent failure,
  * failover execution,
  * recovery and standby re-establishment.
* Measure:

  * failover latency,
  * packet loss during transition,
  * stability of parent selection.

**Deliverables:**

* Validation results
* Measured performance metrics
* Identified edge cases

---

### Milestone 8 — Optimization & Robustness

**Goal:** Improve performance and reliability for real-world usage.

**Scope:**

* Optimize:

  * refresh intervals,
  * failover thresholds,
  * power consumption impact.
* Handle edge cases:

  * rapid oscillation between parents,
  * both parents degrading,
  * network partitions.
* Improve resilience:

  * recovery strategies,
  * watchdog behavior.

**Deliverables:**

* Stable and optimized implementation
* Reduced failover latency and oscillation
* Robust behavior under adverse conditions

---

### Milestone 9 — Documentation & Release

**Goal:** Make the project usable and reproducible.

**Scope:**

* Document:

  * architecture,
  * design decisions,
  * limitations vs. standard Thread behavior.
* Provide:

  * setup instructions,
  * example ESPHome configurations,
  * test results and known issues.

**Deliverables:**

* Complete documentation
* Public repository ready for use and contribution

---

If you want, the next step can be turning **Milestone 3 into a concrete code architecture (files, classes, and interfaces)** so an implementation agent can start coding immediately.

