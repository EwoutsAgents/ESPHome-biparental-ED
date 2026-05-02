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

1. Re-run the same four comparison scenarios on the updated targeted-standby-attach path before considering any deeper sweep (`-8dB`, `-12dB`, `-15dB`).
2. If preferred misses remain high, investigate whether OpenThread parent-search/backoff tuning can improve convergence without forcing disruptive generic reattach.

With the current evidence, the correct closeout is a **negative result for the present policy revision**, not a claimed performance improvement.

## Follow-up implementation: targeted standby attach boundary

After the comparison batch, the standby-switch path was updated to reflect the real control boundary.

### Root cause confirmed

The previous preferred path was only target-aware in the overlay policy/diagnostics layer, while the actual attach request still used public generic APIs.

### Updated implementation direction

The standby path now carries both:

- target standby `RLOC16` (intent + outcome comparison), and
- target standby extended address (`otExtAddress`) captured from `otThreadParentResponseInfo`.

At failover trigger, the component now requests a **targeted standby attach** and logs:

- `Requesting targeted standby attach target_rloc16=0x.... target_ext=...`
- `Targeted standby attach accepted`

Outcome classification remains unchanged in policy terms:

- `Targeted standby outcome=success|miss|timeout`
- miss/timeout still fall back to generic reattach.

### Explicit OpenThread API boundary

Public OpenThread APIs still do not expose "attach to this exact parent". The targeted path therefore uses an isolated private-hook boundary in the adapter (`biparental_ot_request_selected_parent_attach(...)`) to integrate with OpenThread internal selected-parent attach behavior.

If that hook is not available in the current OpenThread build, targeted standby attach is reported unavailable and the controller falls back to generic reattach.

This keeps the limitation explicit: true fast standby switching requires OpenThread selected-parent/internal integration, not only `otThreadSearchForBetterParent()`.

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
- additional config/compile for `examples/milestone-8-variant-b-search-refresh.yaml`
- config/compile checks for Scenario C router configs:
  - `examples/milestone-8-scenario-c-active-parent-router.yaml`
  - `examples/milestone-8-scenario-c-standby-router.yaml`

## Scenario C — active-parent TX ramp-down

Scenario C is a Milestone 8 scenario (not a new firmware variant):

- Variant A = default OpenThread reference ED
- Variant B = biparental ED / targeted standby attach ED

Scenario goal: isolate behavior when only the **current active parent** degrades while standby routers stay healthy.

Scenario definition:

1. Start attached to active parent P1 at `8dB`.
2. Confirm Variant B has standby candidate P2/P3.
3. Keep ED TX fixed.
4. Keep standby router TX fixed at nominal.
5. During a single capture run, decrease only the active parent TX over time: `8dB -> 4dB -> 0dB -> -4dB -> -8dB -> -12dB -> -15dB`.
6. Hold each step for a fixed interval (`STEP_DURATION_SECONDS`, default `30s`) before moving to the next one.
7. Budget per-run timeout to include both hold time and router reflash overhead (`UPLOAD_OVERHEAD_SECONDS`, default `12s`, plus a small final margin).
8. Capture both A and B runs under the same placement and dataset while the active-parent ramp unfolds in-run.

Artifacts:

- `artifacts/milestone-8/active-parent-ramp-down/`

Required metadata per run:

- active parent router identity
- active parent TX mode (`in_run_ramp`)
- active parent TX schedule
- active parent step duration
- active parent upload-overhead budget
- standby router TX power
- ED TX power
- variant A/B
- run number

Variant B metrics to collect:

- standby discovered yes/no
- targeted standby attach requested yes/no
- targeted standby outcome success/miss/timeout
- generic fallback used yes/no
- failover trigger -> stable child time

Variant A metrics to collect:

- detach events
- reattach time
- final parent after recovery
- whether OpenThread switched parent naturally

Interpretation boundary:

Use Scenario C to isolate the intended biparental advantage (pre-discovered standby switch path). Do **not** claim performance improvement until repeated A/C vs B/C runs are collected.

Implementation note: `scripts/milestone8-scenario-c-active-parent-ramp-down.sh` now performs the active-parent TX ramp **during each run** instead of collecting separate fixed-power runs per step.

### Post-fix hardware spot-checks

After the implementation change above, a small follow-up hardware check was run before redoing the full comparison matrix.

#### Nominal short window

Artifact:

- `artifacts/milestone-8/nominal-postfix/B/run-001-router-8dB-ed-8dB.log`

Observed behavior:

- Variant B booted and registered the parent-response callback correctly.
- The short window did **not** exercise a preferred-target handoff.
- It first hit one `reason=no_standby` generic reattach, then recovered `standby=yes` and remained attached.

Interpretation:

- this confirmed the updated firmware was running on hardware,
- but it did **not** answer whether preferred-target misses were solved.

#### Uplink-only micro-batch after the fix

To force the preferred path to execute, a small three-run Variant B micro-batch was captured with routers kept nominal and the ED reflashed at `-4dB`:

- scenario: `uplink-only-postfix`
- router power: `8dB`
- ED power: `-4dB`
- artifacts:
  - `artifacts/milestone-8/uplink-only-postfix/B/run-001-router-8dB-ed--4dB.log`
  - `artifacts/milestone-8/uplink-only-postfix/B/run-002-router-8dB-ed--4dB.log`
  - `artifacts/milestone-8/uplink-only-postfix/B/run-003-router-8dB-ed--4dB.log`

Micro-batch summary:

| Run | Preferred request exercised? | Outcome | Notes |
|---|---|---|---|
| `001` | yes | `miss` | log shows `Requesting best-effort preferred parent search`, `Attach attempt 0, BetterParent`, then generic fallback after reattaching to the original parent |
| `002` | no | n/a | one early `no_standby` generic reattach, then standby recovered and the window ended without a preferred request |
| `003` | yes | `miss` | same pattern as run 001: attached search accepted, BetterParent attempt observed, final attached parent still not equal to target |

Aggregate result from the three-run micro-batch:

- preferred requests exercised: `2`
- preferred successes: `0`
- preferred misses: `2`
- runs with no preferred request during the capture window: `1`

Important log evidence from the exercised runs:

```text
Requesting best-effort preferred parent search (target=0x8c00)
Best-effort preferred parent search accepted (target=0x8c00, staying attached while OpenThread searches)
Attach attempt 0, BetterParent
Preferred search outcome=miss target=0x8c00 attached=0xe000 -> action=generic_reattach
```

and similarly for run `003`:

```text
Requesting best-effort preferred parent search (target=0x9400)
Best-effort preferred parent search accepted (target=0x9400, staying attached while OpenThread searches)
Attach attempt 0, BetterParent
Preferred search outcome=miss target=0x9400 attached=0xe000 -> action=generic_reattach
```

#### Post-fix interpretation

This follow-up hardware evidence sharpens the conclusion:

- the **control-path bug is fixed** — the device now stays attached and OpenThread actually performs `BetterParent` search before fallback;
- however, the **practical Milestone 8 issue is not yet solved** in this setup — the exercised preferred attempts still resolved as `miss`, with recovery continuing through generic reattach.

So the code change corrected the preferred-search mechanics, but it did not yet produce a demonstrated preferred-target switching win on hardware.

## Scenario C ramp-down batch update (longer repeats)

Date: 2026-04-30

A longer Scenario C run set was completed at active-parent TX levels `-4dB`, `-8dB`, `-12dB`, `-15dB` using repeated A/B captures.

Historical note: the batch below was collected with the earlier Scenario C harness that used one fixed active-parent TX level per run. The current runner has since been changed to ramp the active parent during a single run.

### Scenario C in-run ramp rerun (corrected timeout budget)

Date: 2026-05-02

The in-run ramp harness was rerun after increasing the per-run timeout budget to cover both dwell time and router reflash overhead (`duration_seconds=297`, `step_duration=30s`, `upload_overhead=12s`).

New ramp artifacts:

- `artifacts/milestone-8/active-parent-ramp-down/A/run-015-router-ramp-ed-8dB.log`
- `artifacts/milestone-8/active-parent-ramp-down/A/run-016-router-ramp-ed-8dB.log`
- `artifacts/milestone-8/active-parent-ramp-down/B/run-015-router-ramp-ed-8dB.log`
- `artifacts/milestone-8/active-parent-ramp-down/B/run-016-router-ramp-ed-8dB.log`

Short summary:

| Run set | Key result |
|---|---|
| Variant A `015-016` | remained stable in-window (`1` attach, `0` detaches in each run) |
| Variant B `015` | `6` attaches / `5` detaches; targeted outcomes `2 success / 2 miss / 0 timeout`; generic fallback used `2` times |
| Variant B `016` | `6` attaches / `6` detaches; targeted outcomes `2 success / 0 miss / 4 timeout`; generic fallback used `1` time |

Interpretation:

- the corrected timeout budget allowed the full in-run ramp schedule to complete cleanly;
- Variant A again stayed stable through the capture window;
- Variant B produced explicit targeted-standby successes during the in-run ramp scenario, which is a stronger signal than the earlier under-budget ramp attempt;
- however, Variant B still showed substantial detach/reattach churn and mixed outcomes (including timeouts), so this rerun improves evidence of path exercise but does **not** by itself establish an overall performance win.

Primary artifacts:

- `artifacts/milestone-8/active-parent-ramp-down/`
- `artifacts/milestone-8/generated/trial-summary.csv`
- `artifacts/milestone-8/generated/trial-summary.md`
- `artifacts/milestone-8/generated/scenario-c-cross-step-summary.md`
- `artifacts/milestone-8/generated/scenario-c-clean-runs-summary.md`

Included runs for this longer-repeat batch:

- `-4dB`: A/B runs `004-006`
- `-8dB`: A/B runs `007-008`
- `-12dB`: A/B runs `009-010`
- `-15dB`: A/B runs `011-012`

### Clean-run cross-step summary (longer repeats only)

| step | variant | n | standby_yes mean/med | standby_no mean/med | targeted_req mean/med | targeted_success mean/med | targeted_miss mean/med | fallback mean/med |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| -4dB | A | 3 | 0.00/0 | 0.00/0 | 0.00/0 | 0.00/0 | 0.00/0 | 0.00/0 |
| -4dB | B | 3 | 36.67/36 | 13.33/13 | 2.67/3 | 1.33/1 | 2.00/2 | 1.00/1 |
| -8dB | A | 2 | 0.00/0 | 0.00/0 | 0.00/0 | 0.00/0 | 0.00/0 | 0.00/0 |
| -8dB | B | 2 | 32.00/32 | 13.00/13 | 1.50/2 | 1.00/1 | 1.00/1 | 1.00/1 |
| -12dB | A | 2 | 0.00/0 | 0.00/0 | 0.00/0 | 0.00/0 | 0.00/0 | 0.00/0 |
| -12dB | B | 2 | 42.00/42 | 12.00/12 | 3.00/3 | 2.00/2 | 2.00/2 | 1.00/1 |
| -15dB | A | 2 | 0.00/0 | 0.00/0 | 0.00/0 | 0.00/0 | 0.00/0 | 0.00/0 |
| -15dB | B | 2 | 35.00/35 | 14.50/14 | 2.00/2 | 1.00/1 | 2.00/2 | 1.00/1 |

### Observed pattern (descriptive only)

- Variant A parsed fields remain near-zero across these Scenario C ramp-down captures.
- Variant B repeatedly shows standby visibility, targeted request activity, and mixed targeted outcomes with generic fallback commonly used.
- At lower TX levels, Variant B behavior remains active but variable run-to-run (success and miss both present).

Interpretation boundary remains unchanged: these results are promising for behavior characterization, but they are not sufficient for a performance claim without broader repeated A/C vs B/C statistics and pre-declared statistical criteria.
