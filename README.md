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

## Roadmap

- **Milestone 3:** ESPHome external component design + compileable scaffold
- **Milestone 4:** Warm standby implementation
- **Milestone 5:** Failover control + accelerated reattachment
- **Milestone 6:** ESPHome integration + observability
- **Milestone 7:** ESP32-C6 hardware validation
- **Milestone 8:** Optimization + robustness
- **Milestone 9:** Documentation + release
