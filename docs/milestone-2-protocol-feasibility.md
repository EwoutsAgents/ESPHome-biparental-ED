# Milestone 2 — Protocol-Level Feasibility Validation

Status: **draft complete (v0.1)**

## Goal

Validate whether active-passive bi-parental behavior can be implemented as an ESPHome external component overlay **without forking OpenThread**.

## Validated protocol constraints

### C1) Single active parent model

Thread End Devices attach as Child to a Router/REED parent and operate with a single active parent-child relationship at a time.

Implication for this project:
- "Bi-parental" cannot mean two simultaneously active parents.
- Standby must remain metadata/candidate state, not a second attached parent.

### C2) Reattachment is the parent switch mechanism

Thread attach flow is Parent Request → Parent Response(s) → Child ID Request → Child ID Response.

Implication:
- Parent changes must go through a normal attach/reattach sequence.
- Direct "instant parent swap" semantics are out-of-model.

### C3) Parent health and better-parent selection are first-class concerns

OpenThread exposes parent quality and search mechanisms.

Relevant APIs and configs:
- `otThreadGetParentAverageRssi()`
- `otThreadGetParentLastRssi()`
- `otThreadGetParentInfo()`
- `otThreadSearchForBetterParent()` (search while staying attached as child)
- `otThreadRegisterParentResponseCallback()` (requires `OPENTHREAD_CONFIG_MLE_PARENT_RESPONSE_CALLBACK_API_ENABLE`)
- `OPENTHREAD_CONFIG_PARENT_SEARCH_*` config family
- `OPENTHREAD_CONFIG_MLE_ATTACH_BACKOFF_*` config family

Implication:
- Warm-standby logic and accelerated decisioning can be built in an overlay controller using public interfaces + config choices.

## Feasibility outcome

## ✅ What remains compliant (no stack fork)

1. Maintain one active parent + one warmed standby candidate in component state.
2. Continuously score active parent quality (RSSI + supervision/control-plane signals).
3. Refresh candidate knowledge periodically via:
   - parent-response callback data (if enabled), and/or
   - parent-search trigger (`otThreadSearchForBetterParent()`) and resulting attach behavior.
4. Trigger failover by initiating standard detach/reattach path and allowing normal MLE attach primitives.
5. Tune attach responsiveness using documented OpenThread config options (`PARENT_SEARCH_*`, `MLE_ATTACH_BACKOFF_*`) at build/integration time.

## ⚠️ What is partially feasible with public APIs

1. **Preferred-parent biasing**
   - Overlay can rank a preferred standby and decide when to trigger search/reattach.
   - But exact parent selection internals are stack-owned; hard pinning a specific parent via public API is limited.

2. **Fast failover latency floor**
   - Overlay can shorten trigger thresholds/timers and invoke search earlier.
   - Final latency is still bounded by attach procedure, radio conditions, and stack backoff behavior.

## ❌ What would require stack modification / non-compliant shortcuts

1. Forcing parent switch without normal attach exchange.
2. Creating persistent dual-parent child context (active-active child semantics).
3. Injecting private hooks to override internal parent selection and acceptance logic beyond available APIs/configuration.

## Strategy decision (Milestone 2 freeze)

Adopt: **External component overlay (no OpenThread fork)**.

Implementation posture:
- Keep Thread/OpenThread attach path authoritative.
- Use overlay policy only for:
  - health monitoring,
  - candidate ranking,
  - trigger timing,
  - diagnostics.

## Risks and verification items for Milestone 3+

1. Confirm availability of required OpenThread APIs in target ESP-IDF/OpenThread build used by ESPHome on ESP32-C6.
2. Validate whether `otThreadRegisterParentResponseCallback()` can be enabled safely in the intended build profile.
3. Measure practical failover latency impact of `PARENT_SEARCH_*` and `MLE_ATTACH_BACKOFF_*` tuning on real hardware.
4. Confirm energy impact for SED/FED operating modes with parent-search policies.

## Reference links

- Node roles/types (Thread primer):
  - https://openthread.io/guides/thread-primer/node-roles-and-types
- Network discovery + attach flow:
  - https://openthread.io/guides/thread-primer/network-discovery
- Periodic parent search feature:
  - https://openthread.io/guides/build/features/periodic-parent-search
- Parent search config macros:
  - https://openthread.io/reference/config/group/config-parent-search
- MLE config macros (attach backoff, callbacks):
  - https://openthread.io/reference/config/group/config-mle
- General Thread API (parent RSSI/info/search/parent-response callback):
  - https://openthread.io/reference/group/api-thread-general
