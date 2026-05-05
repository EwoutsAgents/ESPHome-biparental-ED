# Milestone 8 trial summary (initial hardware start)

Date: 2026-04-28
ESPHome: 2026.4.1

## Device mapping used

- ED: `/dev/ttyACM0` (`58:8c:81:ff:fe:5f:d9:28`)
- Routers initially: `/dev/ttyACM1`, `/dev/ttyACM2`
- Routers after milestone-7 mapping correction: `/dev/ttyACM2`, `/dev/ttyACM3`, `/dev/ttyACM4`

## Firmware flashed

- Routers: `examples/link-degradation-router.yaml`, nominal `thread_output_power=8dB`
- Variant A ED: `examples/milestone-8-variant-a-reference-ed.yaml`, nominal `thread_output_power=8dB`
- Variant B ED: `examples/milestone-8-variant-b-biparental-ed.yaml`, nominal `thread_output_power=8dB`

## Initial sanity results

| Variant | Scenario | Run | Result |
|---|---|---:|---|
| A | steady-state, 8dB/8dB | 001 | Attached as child quickly (`Role detached -> child`) |
| B | steady-state, 8dB/8dB | 001 | Attached, but `standby=no`; triggered `no_standby` generic failover |
| B | steady-state, 8dB/8dB, routers on `/dev/ttyACM2/3/4` | 002 | Still `standby=no`; 4 generic reattach/failover events in 180s |

## Blocker

Variant B does not currently satisfy the Milestone 8 precondition for a fair biparental comparison: at nominal power with multiple routers present, it never reports `standby=yes` and repeatedly forces `no_standby` reattach cycles. Degradation sweeps are paused to avoid collecting misleading comparison data.

Key evidence from `steady-state/B/run-002-router-8dB-ed-8dB.log`:

```text
standby=yes 0
standby=no 50
Requesting generic reattach/failover 4
Role child -> detached 4
Role detached -> child 4
```

## Search-refresh reproduction attempt

Added temporary neighbor-scan diagnostics and tested `examples/milestone-8-variant-b-search-refresh.yaml` with:

- `parent_search_refresh_interval: 10s`
- `hard_failure_timeout: 60s`

Result over 150s:

```text
neighbor candidate lines: 0
neighbor seen=0 lines: 15
standby=yes: 0
standby=no: 31
better-parent refresh: 15
generic failover: 0
```

OpenThread did run BetterParent attach attempts, but the ED neighbor scan still returned no alternate router neighbors.

## Parent-response callback fix

Root cause narrowed down: on the MED/MTD ED, `otThreadGetNextNeighborInfo()` does not expose alternate router parents, even though all routers are FTD-capable and individually attachable. The OpenThread parent-response callback is the correct discovery source.

Changes made:

- Enabled `OPENTHREAD_CONFIG_MLE_PARENT_RESPONSE_CALLBACK_API_ENABLE=1` for ED builds in `examples/hwtest-ed.yaml`.
- Registered `otThreadRegisterParentResponseCallback()` from `biparental_ed` once the OpenThread lock is ready.
- Fed callback-derived parent responses into `CandidateManager` using RSSI plus a conservative link-quality-derived margin proxy.

Nominal verification: `artifacts/milestone-8/parent-callback/B/run-001-router-8dB-ed-8dB.log`

```text
parent_response: 167
standby=yes: 52
standby=no: 1
neighbor_scan candidate: 0
neighbor seen=0: 25
security warnings: 0
registered callback: 1
```

Conclusion: Variant B nominal `8dB/8dB` now satisfies the precondition for Milestone 8 sweeps (`standby=yes`). Neighbor-table diagnostics still show zero alternate router neighbors, confirming callback-based discovery is required for this ED mode.

## Baseline/degradation resume after diagnostic cleanup

Temporary verbose neighbor/parent-response diagnostics were cleaned from the component while keeping callback-derived candidate observation.

| Variant | Scenario | Run | Router power | ED power | Result |
|---|---|---:|---:|---:|---|
| A | `nominal-callback-baseline` | 001 | 8dB | 8dB | Reference ED attached as child; no biparental diagnostics expected. |
| B | `nominal-callback-baseline` | 001 | 8dB | 8dB | Invalid/stale cached build; callback registration log absent; kept as a negative control artifact. |
| B | `nominal-callback-baseline` | 002 | 8dB | 8dB | Clean rebuild; callback registered; `standby=yes` recovered and parent switching/failover activity observed. |
| B | `uplink-only` | 001 | 8dB | -4dB | Clean rebuild; callback registered; `standby=yes` observed under ED TX-power reduction. |

Key counts:

```text
nominal-callback-baseline/A/run-001-router-8dB-ed-8dB.log
  Output power: 8dBm
  Role detached -> child: 1

nominal-callback-baseline/B/run-002-router-8dB-ed-8dB.log
  Output power: 8dBm
  Registered OpenThread parent response callback: 1
  diag lines: 48
  standby=yes: 29
  standby=no: 19
  preferred outcome lines: 4
  generic failover requests: 1
  preferred failover requests: 2
  final: standby=yes active=0x8c00 standby=0xe000 failovers=3 preferred(a/s/m)=2/0/2

uplink-only/B/run-001-router-8dB-ed--4dB.log
  Output power: -4dBm
  Registered OpenThread parent response callback: 1
  diag lines: 48
  standby=yes: 31
  standby=no: 17
  preferred outcome lines: 2
  generic failover requests: 2
  preferred failover requests: 1
  final: standby=yes active=0xe000 standby=0x8c00 failovers=3 preferred(a/s/m)=1/0/1
```

Additional B fixed-power sweeps were then captured after reflashing the routers at `-4dB` with `examples/link-degradation-router.yaml`, restoring that YAML to `8dB`, and reflashing the ED as needed.

| Variant | Scenario | Run | Router power | ED power | Result |
|---|---|---:|---:|---:|---|
| B | `downlink-only` | 001 | -4dB | 8dB | Routers reduced, ED nominal; callback registered; `standby=yes` observed; preferred target attempts missed and fell back via generic reattach. |
| B | `symmetric` | 001 | -4dB | -4dB | Routers and ED reduced; callback registered; `standby=yes` observed; preferred target attempts missed and fell back via generic reattach. |
| A | `downlink-only` | 001 | -4dB | 8dB | Reference ED attached and remained attached; 1 `Role detached -> child`, 0 child detaches. |
| A | `uplink-only` | 001 | 8dB | -4dB | Reference ED attached and remained attached; 1 `Role detached -> child`, 0 child detaches. |
| A | `symmetric` | 001 | -4dB | -4dB | Reference ED attached and remained attached; 1 `Role detached -> child`, 0 child detaches. |

Additional key counts:

```text
downlink-only/B/run-001-router--4dB-ed-8dB.log
  Output power: 8dBm
  Registered OpenThread parent response callback: 1
  diag lines: 50
  standby=yes: 36
  standby=no: 14
  preferred outcome lines: 4
  generic failover requests: 3
  preferred failover requests: 2
  parent-response security warnings: 3
  final: standby=yes active=0xe000 standby=0x8c00 failovers=3 preferred(a/s/m)=2/0/2

symmetric/B/run-001-router--4dB-ed--4dB.log
  Output power: -4dBm
  Registered OpenThread parent response callback: 1
  diag lines: 51
  standby=yes: 35
  standby=no: 16
  preferred outcome lines: 4
  generic failover requests: 3
  preferred failover requests: 2
  parent-response security warnings: 3
  final: standby=yes active=0xe000 standby=0x1800 failovers=3 preferred(a/s/m)=2/0/2
```

Current first-pass coverage includes Variant B nominal/uplink-only/downlink-only/symmetric and matching Variant A degraded reference captures at the `-4dB` degradation point. Routers and YAML substitutions were restored to nominal `8dB`, and the ED was reflashed back to Variant B nominal `8dB` afterward. Remaining comparison work: repeat B/A trials for statistics and decide whether to continue the deeper power sweep (`-8dB`, `-12dB`, `-15dB`).

## Scenario C — active-parent TX ramp-down

Scenario C isolates active-parent degradation while keeping standby routers healthy:

- Variant naming remains unchanged:
  - Variant A = OpenThread reference ED
  - Variant B = biparental ED / targeted standby attach ED
- Scenario C = active-parent-only TX ramp-down (not a new variant)

Definition:

1. ED starts attached to active parent P1.
2. Variant B confirms a standby candidate (P2/P3) is discovered.
3. ED TX power remains fixed.
4. Standby router TX power remains fixed at nominal.
5. Only active parent TX power is stepped:
   `8dB -> 4dB -> 0dB -> -4dB -> -8dB -> -12dB -> -15dB`.
6. A and B runs are captured at each step under same placement and dataset.

Artifacts live under:

- `artifacts/milestone-8/active-parent-ramp-down/`

Metadata per trial includes:

- active parent router identity,
- active parent TX power,
- standby router TX power,
- ED TX power,
- variant,
- run id.

Interpretation note:

Scenario C is intended to isolate the biparental design advantage: switching to a pre-discovered standby when the active parent degrades, rather than relying on generic rediscovery. Do **not** claim performance improvement until repeated A/C vs B/C runs are collected.

## Post-fix spot-checks after preferred-search implementation change

Date: 2026-04-29

After updating the preferred path so `request_failover_to_preferred()` stays attached during `otThreadSearchForBetterParent()`, a small hardware follow-up was run to see whether the previous preferred-`miss` issue was resolved in practice.

### Nominal short-window check

Artifact:

- `artifacts/milestone-8/nominal-postfix/B/run-001-router-8dB-ed-8dB.log`

Result:

- firmware booted with the updated callback + preferred-search logic;
- the window did not trigger a preferred handoff;
- one early `no_standby` generic reattach occurred, after which standby recovered and the node stayed attached.

Interpretation: useful as a smoke check of the updated firmware on the bench, but not enough by itself to answer the preferred-`miss` question.

### Uplink-only three-run micro-batch

To force the preferred path to run, the ED was reflashed at `-4dB` while routers stayed nominal `8dB`.

Artifacts:

- `artifacts/milestone-8/uplink-only-postfix/B/run-001-router-8dB-ed--4dB.log`
- `artifacts/milestone-8/uplink-only-postfix/B/run-002-router-8dB-ed--4dB.log`
- `artifacts/milestone-8/uplink-only-postfix/B/run-003-router-8dB-ed--4dB.log`

Per-run outcome:

| Run | Preferred request | Outcome | Summary |
|---|---|---|---|
| `001` | yes | `miss` | best-effort preferred parent search accepted; `Attach attempt 0, BetterParent` observed; attached parent still returned to the original parent, then generic fallback recovered |
| `002` | no | n/a | one early `no_standby` generic reattach; standby recovered; no preferred request issued before the window ended |
| `003` | yes | `miss` | same pattern as run `001`, with attached search accepted and BetterParent attempted before generic fallback |

Aggregate:

```text
preferred requests exercised: 2
preferred successes:          0
preferred misses:             2
runs without preferred req:   1
```

Representative evidence from the exercised runs:

```text
Requesting best-effort preferred parent search (target=0x8c00)
Best-effort preferred parent search accepted (target=0x8c00, staying attached while OpenThread searches)
Attach attempt 0, BetterParent
Preferred outcome=miss target=0x8c00 attached=0xe000 -> action=generic_reattach
```

and:

```text
Requesting best-effort preferred parent search (target=0x9400)
Best-effort preferred parent search accepted (target=0x9400, staying attached while OpenThread searches)
Attach attempt 0, BetterParent
Preferred outcome=miss target=0x9400 attached=0xe000 -> action=generic_reattach
```

### Updated takeaway

These post-fix spot-checks confirm two things at once:

1. the implementation fix is real — the device now stays attached and OpenThread performs `BetterParent` search before fallback;
2. the original Milestone 8 practical issue is **not yet resolved on hardware** — the preferred attempts exercised in this follow-up still ended as `miss` and recovered through generic reattach.

The ED was restored to the nominal Variant B image after the micro-batch.

## Scenario C longer-repeat ramp-down update (2026-04-30)

Additional Scenario C active-parent ramp-down runs were captured and summarized for:

- `-4dB` (runs `004-006`)
- `-8dB` (runs `007-008`)
- `-12dB` (runs `009-010`)
- `-15dB` (runs `011-012`)

Key generated artifacts:

- `artifacts/milestone-8/generated/trial-summary.csv`
- `artifacts/milestone-8/generated/trial-summary.md`
- `artifacts/milestone-8/generated/scenario-c-cross-step-summary.md`
- `artifacts/milestone-8/generated/scenario-c-clean-runs-summary.md`

Clean-run summary (longer-repeat batches only):

| step | variant | n | standby_yes mean/med | targeted_req mean/med | targeted_success mean/med | targeted_miss mean/med | fallback mean/med |
|---|---:|---:|---:|---:|---:|---:|---:|
| -4dB | A | 3 | 0.00/0 | 0.00/0 | 0.00/0 | 0.00/0 | 0.00/0 |
| -4dB | B | 3 | 36.67/36 | 2.67/3 | 1.33/1 | 2.00/2 | 1.00/1 |
| -8dB | A | 2 | 0.00/0 | 0.00/0 | 0.00/0 | 0.00/0 | 0.00/0 |
| -8dB | B | 2 | 32.00/32 | 1.50/2 | 1.00/1 | 1.00/1 | 1.00/1 |
| -12dB | A | 2 | 0.00/0 | 0.00/0 | 0.00/0 | 0.00/0 | 0.00/0 |
| -12dB | B | 2 | 42.00/42 | 3.00/3 | 2.00/2 | 2.00/2 | 1.00/1 |
| -15dB | A | 2 | 0.00/0 | 0.00/0 | 0.00/0 | 0.00/0 | 0.00/0 |
| -15dB | B | 2 | 35.00/35 | 2.00/2 | 1.00/1 | 2.00/2 | 1.00/1 |

Descriptive takeaway: Variant B continues to show standby discovery and targeted activity across deeper active-parent TX reductions, with mixed targeted outcomes and frequent generic fallback; Variant A parsed Scenario C failover/targeted fields remain near zero in these captures.

## Scenario C in-run ramp reruns after ext-address retention fix (2026-05-02)

After fixing Variant B standby ext-address retention, the in-run ramp scenario was rerun with the updated harness that now includes the finer low-end sweep (`-13dB`, `-14dB`) and final `off` step. The first post-fix rerun was `017`, followed by additional confirmation reruns `018-020`.

Artifacts:

- `artifacts/milestone-8/active-parent-ramp-down/A/run-017-router-ramp-ed-8dB.log`
- `artifacts/milestone-8/active-parent-ramp-down/B/run-017-router-ramp-ed-8dB.log`
- `artifacts/milestone-8/active-parent-ramp-down/A/run-018-router-ramp-ed-8dB.log`
- `artifacts/milestone-8/active-parent-ramp-down/B/run-018-router-ramp-ed-8dB.log`
- `artifacts/milestone-8/active-parent-ramp-down/A/run-019-router-ramp-ed-8dB.log`
- `artifacts/milestone-8/active-parent-ramp-down/B/run-019-router-ramp-ed-8dB.log`
- `artifacts/milestone-8/active-parent-ramp-down/A/run-020-router-ramp-ed-8dB.log`
- `artifacts/milestone-8/active-parent-ramp-down/B/run-020-router-ramp-ed-8dB.log`

Short summary:

| Run set | Key result |
|---|---|
| Variant A `017-020` | all four reruns remained stable in-window (`1` attach, `0` detaches each) |
| Variant B `017` | `10` attaches / `9` detaches; targeted outcomes `8 success / 0 miss / 0 timeout`; generic fallback used `1` time |
| Variant B `018` | `8` attaches / `7` detaches; targeted outcomes `2 success / 6 miss / 0 timeout`; generic fallback used `2` times |
| Variant B `019` | `9` attaches / `8` detaches; targeted outcomes `6 success / 2 miss / 0 timeout`; generic fallback used `1` time |
| Variant B `020` | `8` attaches / `7` detaches; targeted outcomes `5 success / 2 miss / 0 timeout`; generic fallback used `1` time |

Before/after evidence for the specific bug fix:

- Earlier corrected-timeout runs still showed targeted requests launched with incomplete identity:
  - `B run-015`: `target_ext=unknown` followed by `Targeted standby attach rejected: missing preferred extended address` for `0xe000`
  - `B run-016`: the same rejection pattern repeated for `0xe000`, `0x1800`, and `0x8c00`
- In post-fix runs `017-020`, no observed `target_ext=unknown` requests or `missing preferred extended address` rejections remained.
- In `B run-017`, targeted requests consistently carried concrete ext-addresses, for example:
  - `target_rloc16=0x1800 target_ext=da:97:55:79:43:a0:5a:ac`
  - `target_rloc16=0xe000 target_ext=fa:cf:27:a3:d9:37:7f:d9`

Interpretation:

- the ext-address retention fix materially improved targeted-standby execution in Scenario C;
- the milestone narrative should now reflect repeated targeted successes rather than a one-off post-fix rerun;
- however, Variant B still shows substantial detach/reattach churn and still falls back generically in most of these reruns, so this evidence does **not** overturn the broader Milestone 8 negative A-vs-B closeout.
