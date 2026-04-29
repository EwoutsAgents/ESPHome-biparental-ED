# Milestone 8 — Parent Switching Performance Comparison

Status: **completed with a negative result for the current policy revision**

## Goal

Quantify parent-switching performance of:

1. **Reference baseline**: default Thread/OpenThread behavior (no biparental policy overlay)
2. **Novel approach**: `biparental_ed` active + warm-standby policy

The objective is to measure whether the biparental approach improves recovery/switch behavior under controlled conditions, and at what trade-offs.

## Scope

In scope:

- Parent-switch and reattach performance comparison on the same hardware setup.
- Timing, success-rate, and stability metrics.
- Scenario-based comparison (degradation/failure-triggered switching).

Out of scope (for this milestone):

- New failover algorithm changes.
- Production tuning changes to existing milestone behavior.
- Release packaging.

## Test variants

### Variant A — Default Thread reference

A comparable ESPHome image using standard OpenThread behavior only (without `biparental_ed` failover policy logic).

### Variant B — Biparental policy

ESPHome image with `biparental_ed` enabled and current milestone-7-validated behavior.

## Fair-comparison controls

- Same ED hardware type and firmware build settings where possible.
- Same router set and placement.
- Same channel/PAN/dataset.
- Same trigger scripts and disturbance schedule.
- Alternate run order (A/B/A/B...) to reduce drift bias.
- Repeated trials per scenario (target: at least 20, preferred 30+).

## Scenarios to compare

1. **Active parent outage**
   - Force active-parent unavailability (e.g., router reset/power cut).
2. **Link degradation path**
   - Trigger degraded conditions long enough to cross policy thresholds.
   - Use controlled OpenThread TX-power reduction as the primary static-environment degradation method.
   - Vary router/FTD `openthread.output_power` to weaken router -> ED direction.
   - Vary ED/MTD `openthread.output_power` to weaken ED -> router direction.
   - Run separate downlink-only, uplink-only, and symmetric low-power cases before interpreting full failover behavior.
3. **Preferred-target unavailable / miss-like conditions**
   - Conditions where selected candidate cannot be used reliably.
4. **Steady-state sanity**
   - No forced disruption; ensure both variants maintain stable attachment.

## Metrics

Primary:

- **T_detach_to_child_ms**: time from detach event to regaining `Role detached -> child`.
- **T_failover_window_ms**: time from failover trigger/request to post-reattach stable state.
- **Switch success rate**: fraction of trials that reattach successfully within timeout budget.

Secondary:

- Parent-switch outcome distribution (success/miss/timeout where applicable).
- Failover reason distribution (`no_standby`, `preferred_miss`, `preferred_timeout`, etc.).
- Retry/attach-attempt counts during transition.
- Post-switch stability window (short-term re-detach frequency).

## Controlled TX-power degradation setup

Because the device placement and test environment are expected to remain static, link degradation can be forced reproducibly by reducing OpenThread radio output power instead of physically moving devices or adding variable obstructions.

ESPHome supports this directly through the OpenThread component:

```yaml
openthread:
  output_power: ${thread_output_power}
```

Initial smoke-test YAML examples were added for fixed-power degradation builds:

- `examples/tx-power-degradation.yaml` — reusable package setting `openthread.output_power`
- `examples/link-degradation-router.yaml` — router/FTD degradation test image
- `examples/link-degradation-ed.yaml` — ED/MTD degradation test image with `biparental_ed` enabled

Validation performed:

```text
esphome config link-degradation-router.yaml  ✅
esphome config link-degradation-ed.yaml      ✅
esphome compile link-degradation-router.yaml ✅
esphome compile link-degradation-ed.yaml     ✅
```

Notes:

- On ESP32-C6, ESPHome validates OpenThread `output_power` in the `-15dB..20dB` range.
- `output_power` is applied at firmware startup, so fixed-power sweeps require rebuilding/reflashing per power level.
- If true runtime ramping is later required, add a dedicated runtime control path; do not use that unless the static fixed-power sweep is insufficient.

Recommended first sweep:

```text
baseline: router 8dB,  ED 8dB
downlink: router steps down, ED fixed at 8dB
uplink:   ED steps down, router fixed at 8dB
symmetric: router and ED both step down together
```


## Working setup decisions

Prepared reviewable setup for Milestone 8 without flashing devices or running trials.

Variant configs:

- **Variant A — default OpenThread reference:** `examples/milestone-8-variant-a-reference-ed.yaml`
  - Standalone MTD image.
  - Does **not** load `external_components` or configure `biparental_ed`.
  - Uses the same Thread dataset, ESP32-C6 board, ESP-IDF framework, logger, IPv6, mDNS, and default `thread_output_power` as the existing hardware ED example.
- **Variant B — biparental policy:** `examples/milestone-8-variant-b-biparental-ed.yaml`
  - Packages `examples/hwtest-ed.yaml` plus `examples/tx-power-degradation.yaml`.
  - Keeps the current milestone-7 hardware-tested `biparental_ed` policy settings.

Initial fixed TX-power sweep levels:

```text
nominal:  8dB
steps:    4dB, 0dB, -4dB, -8dB, -12dB, -15dB
```

Scenario matrix for first pass:

```text
steady-state sanity: router 8dB, ED 8dB
downlink-only:       router in [4, 0, -4, -8, -12, -15] dB; ED fixed at 8dB
uplink-only:         ED in [4, 0, -4, -8, -12, -15] dB; router fixed at 8dB
symmetric:           router and ED both step together through [4, 0, -4, -8, -12, -15] dB
```

Rationale:

- `8dB` is retained as the nominal baseline used by the current degradation examples.
- The sweep uses coarse 4 dB steps down to `-12dB`, then includes the ESP32-C6 minimum `-15dB` validation boundary.
- Each fixed-power point requires a separate build/flash because ESPHome applies `openthread.output_power` at firmware startup.

Repeatable collection helpers:

- `scripts/milestone8-validate.sh` validates the Milestone 8 ED variants plus the router degradation image; pass `--compile` for firmware builds. Set `ESPHOME_BIN` when `esphome` is not on `PATH`.
- `scripts/milestone8-log-trial.sh` writes per-trial metadata and ESPHome logs under `artifacts/milestone-8/`. It does not flash firmware.

Milestone 8 setup validation performed: config and compile succeeded for `examples/milestone-8-variant-a-reference-ed.yaml`, `examples/milestone-8-variant-b-biparental-ed.yaml`, and `examples/link-degradation-router.yaml` using ESPHome 2026.4.1. No flashing or benchmark trials were run.

Commands used during setup/smoke test:

```text
git status --short
grep -R "output_power" .../site-packages/esphome/components/openthread
scripts/milestone8-validate.sh
ESPHOME_BIN=/home/ewout/.openclaw/workspace-softwaredeveloper/.venv-esphome/bin/esphome scripts/milestone8-validate.sh --compile
```

## Instrumentation and evidence

Planned sources:

- ESPHome serial logs with timestamps.
- Existing diagnostics lines (`state=...`, `preferred(...)`, `entity.last_failover_reason=...`).
- Trial metadata file: variant, scenario, run id, start/end markers.

Planned artifacts:

- Raw logs per trial set in `/tmp` and/or `artifacts/` paths.
- Parsed summary table (CSV/Markdown).
- Milestone report with median/p95 and pass/fail interpretation.


## Initial hardware start — 2026-04-28

Flashing and initial sanity benchmarking were started after approval. No TX-power degradation sweep was continued because the biparental nominal sanity precondition failed.

Device mapping used:

- ED: `/dev/ttyACM0`
- Routers: `/dev/ttyACM2`, `/dev/ttyACM3`, `/dev/ttyACM4` after checking Milestone 7 hardware mapping.

Commands/artifacts:

- Router/ED flash logs: `artifacts/milestone-8/flash-*.log`
- Trial logs: `artifacts/milestone-8/steady-state/`
- Summary: `artifacts/milestone-8/summary.md`

Sanity observations:

- Variant A reference ED at `8dB/8dB` attached quickly (`Role detached -> child`).
- Variant B at `8dB/8dB` attached, but never acquired a standby parent and repeatedly forced generic reattach/failover with `reason=no_standby`.
- Repeating Variant B with three routers on the Milestone 7 router ports still produced `standby=yes 0`, `standby=no 50`, and 4 reattach cycles in a 180s capture.

Decision: pause degradation sweeps until the nominal Variant B `standby=yes` precondition is restored, otherwise parent-switching performance data would measure a setup failure rather than the intended warm-standby policy.

## Callback discovery finding and first fixed-power results — 2026-04-28

Nominal Variant B initially failed the warm-standby precondition: the MED/MTD ED attached but reported `standby=no` because `otThreadGetNextNeighborInfo()` did not expose alternate router parents on the ED. The fix was to enable/register `otThreadRegisterParentResponseCallback()` and feed callback-derived Parent Response observations into `CandidateManager`.

Important build caveat: one post-cleanup run (`artifacts/milestone-8/nominal-callback-baseline/B/run-001-router-8dB-ed-8dB.log`) was invalid because ESPHome reused stale cached firmware without the callback registration log. A forced clean rebuild produced the valid baseline in run `002`.

First valid Variant B artifacts:

| Scenario | Artifact | Router TX | ED TX | Standby result | Switch notes |
|---|---|---:|---:|---|---|
| Nominal | `artifacts/milestone-8/nominal-callback-baseline/B/run-002-router-8dB-ed-8dB.log` | 8dB | 8dB | 29 yes / 19 no | callback registered; preferred attempts missed; generic fallback recovered |
| Uplink-only | `artifacts/milestone-8/uplink-only/B/run-001-router-8dB-ed--4dB.log` | 8dB | -4dB | 31 yes / 17 no | callback registered; failovers observed |
| Downlink-only | `artifacts/milestone-8/downlink-only/B/run-001-router--4dB-ed-8dB.log` | -4dB | 8dB | 36 yes / 14 no | routers reflashed at reduced TX; preferred attempts missed; generic fallback recovered |
| Symmetric | `artifacts/milestone-8/symmetric/B/run-001-router--4dB-ed--4dB.log` | -4dB | -4dB | 35 yes / 16 no | routers and ED reduced; preferred attempts missed; generic fallback recovered |

Variant A reference artifacts:

- `artifacts/milestone-8/nominal-callback-baseline/A/run-001-router-8dB-ed-8dB.log` — reference ED attached as child; no biparental diagnostics expected.
- `artifacts/milestone-8/downlink-only/A/run-001-router--4dB-ed-8dB.log` — 1 `Role detached -> child`, 0 `Role child -> detached`, output power `8dBm`.
- `artifacts/milestone-8/uplink-only/A/run-001-router-8dB-ed--4dB.log` — 1 `Role detached -> child`, 0 `Role child -> detached`, output power `-4dBm`.
- `artifacts/milestone-8/symmetric/A/run-001-router--4dB-ed--4dB.log` — 1 `Role detached -> child`, 0 `Role child -> detached`, output power `-4dBm`.

After the first-pass captures, `examples/link-degradation-router.yaml`, `examples/milestone-8-variant-a-reference-ed.yaml`, and `examples/milestone-8-variant-b-biparental-ed.yaml` were restored to `thread_output_power: "8dB"`; routers were reflashed nominal and the ED was reflashed back to Variant B nominal.

Interpretation so far: the parent-response callback restores the Milestone 8 precondition (`standby=yes`) across nominal and first `-4dB` degradation cases. The first fixed-power B runs show standby discovery is robust enough to continue benchmarking, but preferred-target switching often reattaches to the original parent (`outcome=miss`) before generic fallback. More repeated A/B trials are needed before making performance claims.

Generated artifact summaries are available under:

- `artifacts/milestone-8/generated/trial-summary.md`
- `artifacts/milestone-8/generated/trial-summary.csv`

## Current comparison snapshot (updated after repeated batch)

Comparison-grade repeated captures now exist for the four main scenarios:

- `nominal-callback-baseline` — Variant A `n=7`, Variant B `n=7` valid runs (`B run-001` excluded as stale cached firmware)
- `downlink-only` — Variant A `n=4`, Variant B `n=4`
- `uplink-only` — Variant A `n=4`, Variant B `n=4`
- `symmetric` — Variant A `n=4`, Variant B `n=4`

### Comparison table

| Scenario | Variant A result | Variant B result | Direct comparison |
|---|---|---|---|
| Nominal `8dB/8dB` | `n=7`; median initial attach `637ms`; `0/7` runs detached after attach | `n=7`; median initial attach `638ms`; median detach->child recovery `1481ms`; total detaches `24`; preferred requests `8`; preferred misses `8/8`; standby observed in `7/7` runs | Baseline stayed attached in-window; biparental variant repeatedly detached/reattached and did not achieve a successful preferred handoff in valid nominal runs. |
| Downlink-only `-4dB/8dB` | `n=4`; median initial attach `629ms`; `0/4` runs detached after attach | `n=4`; median initial attach `631ms`; median detach->child recovery `1109.5ms`; total detaches `13`; preferred requests `4`; preferred misses `4/4`; standby observed in `4/4` runs | Under router TX reduction, baseline again stayed attached while the biparental variant repeatedly recovered through reattach cycles. |
| Uplink-only `8dB/-4dB` | `n=4`; median initial attach `629ms`; `0/4` runs detached after attach | `n=4`; median initial attach `612.5ms`; median detach->child recovery `1501.5ms`; total detaches `13`; preferred requests `4`; preferred misses `4/4`; standby observed in `4/4` runs | With ED TX reduction, initial attach remained similar, but biparental behavior still converged via repeated detach/reattach rather than a successful preferred switch. |
| Symmetric `-4dB/-4dB` | `n=4`; median initial attach `639.5ms`; `0/4` runs detached after attach | `n=4`; median initial attach `633ms`; median detach->child recovery `1193.5ms`; total detaches `13`; preferred requests `4`; preferred misses `4/4`; standby observed in `4/4` runs | Even with both sides reduced, baseline stayed attached while biparental continued to recover, but only through disruptive reattach behavior. |

### Aggregate interpretation

- **Variant A reference** was stable in all repeated comparison windows collected here: it attached quickly (`~0.63s`) and recorded zero post-attach child detaches across the comparison-grade runs above.
- **Variant B biparental** consistently discovered a standby candidate after the parent-response callback fix, but still showed repeated failover churn:
  - nominal median recovery around `1481ms`
  - downlink-only median recovery around `1109.5ms`
  - uplink-only median recovery around `1501.5ms`
  - symmetric median recovery around `1193.5ms`
- Across these comparison-grade repeated runs, preferred failover requests ended in **`miss`** every time they were attempted (`8/8` nominal, `4/4` downlink-only, `4/4` uplink-only, `4/4` symmetric), after which recovery depended on generic reattach behavior.
- The current data therefore does **not** show a performance win for the biparental policy versus the default reference baseline under these scenarios. It does show that the current biparental policy can recover attachment, but at the cost of repeated detach/reattach cycles that the reference baseline did not exhibit in the same capture windows.

## Closeout status

This milestone is closed on the current comparison batch because:

1. Both variants were executed across the four primary comparison scenarios used for the milestone decision.
2. Timing, detach/reattach, standby, and preferred-outcome metrics are published in reproducible artifacts.
3. The major anomalies/confounders are documented, including the stale cached nominal Variant B run and the callback-discovery dependency.
4. A recommendation is now recorded based on measured outcomes.

The repeat count is still below the originally preferred `20-30+` target, so these numbers should be treated as a strong decision batch rather than a final statistical characterization of all environments.

## Risks / caveats

- Serial/upload instability may affect throughput of repeated hardware runs.
- RF environment noise can bias absolute timings; controlled ordering is required.
- Baseline/reference config must remain truly comparable (avoid accidental policy leakage).
- `openthread.output_power` is static per firmware image; repeated reflashing can add test overhead and should be included in run planning.
- TX-power reduction is a controlled link-budget proxy, not a perfect substitute for every real-world RF impairment.
- One nominal Variant B artifact (`nominal-callback-baseline/B/run-001-router-8dB-ed-8dB.log`) remains a known invalid stale-build negative-control run and is excluded from the updated nominal comparison table above.

## Deliverables

- `docs/milestone-8-parent-switching-performance.md` (this final comparison and closeout document)
- `artifacts/milestone-8/generated/trial-summary.md`
- `artifacts/milestone-8/generated/trial-summary.csv`
- Scenario run logs under `artifacts/milestone-8/`

## Final recommendation

Based on the repeated nominal and `-4dB` comparison batch, **do not keep the current biparental policy unchanged**.

Current conclusion: the parent-response callback fix successfully restored standby discovery, but the current policy revision still does not produce a demonstrated switching-performance win over the default Thread reference behavior. Preferred failover continues to resolve as `miss`, and recovery currently depends on disruptive generic reattach behavior.

Recommended follow-up outside this milestone:

1. Re-run the same four comparison scenarios on the updated best-effort preferred-parent-search path before considering any deeper sweep (`-8dB`, `-12dB`, `-15dB`).
2. If preferred misses remain high, investigate whether OpenThread parent-search/backoff tuning can improve convergence without forcing disruptive generic reattach.

With the current evidence, the correct closeout is a **negative result for the present policy revision**, not a claimed performance improvement.

## Follow-up implementation: best-effort preferred parent search

A targeted control-path fix was implemented after the comparison batch above.

### Root cause confirmed

The previous preferred path was only target-aware in the overlay policy and diagnostics layers:

- `FailoverController` stored a preferred target RLOC16 and classified the eventual outcome as `success`, `miss`, or `timeout` based on the attached parent.
- `CandidateManager` maintained the preferred standby candidate identity.
- But `EspHomeOpenThreadPlatformAdapter::request_failover_to_preferred()` did **not** pass that target into OpenThread parent selection. It started `otThreadSearchForBetterParent()` and then immediately detached, so the actual next parent choice remained fully stack-controlled.

### Implemented fix

The preferred request path now behaves as a **best-effort preferred parent search**:

- when already attached as a child and not already on the preferred parent, it starts `otThreadSearchForBetterParent()` and stays attached;
- it no longer immediately detaches after starting that search;
- existing Milestone 5 outcome handling is unchanged: success still requires exact attached parent RLOC16 match, while miss/timeout still fall back to generic reattach.

### Limitation kept explicit

Deterministic RLOC16 targeting is still not possible with the public OpenThread APIs used here:

- `otThreadSearchForBetterParent()` has no target-RLOC16 argument;
- public APIs expose parent search and attach state, but the final parent acceptance decision stays inside OpenThread.

Because of that limitation, logs and docs now describe this path as **best-effort preferred parent search**, not deterministic preferred reattach.

### Validation steps for this fix

Host-side logic validation:

- compile and run `tests/host/test_preferred_failover.cpp`
- verifies preferred requests stay in search-without-detach mode while attached as a child;
- verifies timeout/miss still trigger generic fallback;
- verifies success still requires exact attached-parent RLOC16 match.

ESPHome validation:

- `scripts/milestone8-validate.sh`
- `scripts/milestone8-validate.sh --compile`
- additional component-bearing config validation/compile for `examples/link-degradation-ed.yaml`
