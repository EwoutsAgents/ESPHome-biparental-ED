# Milestone 9 — Incremental Improvements

Status: **started / in progress**

## Goal

Improve the current `biparental_ed` behavior incrementally after the Milestone 8 comparison closeout.

This milestone does **not** assume a large redesign. Instead, it focuses on targeted improvements that reduce instability, improve targeted-standby reliability, and make follow-up measurement easier.

## Why this milestone exists

Milestone 8 established two important points:

1. the current policy revision did **not** yet demonstrate an overall switching-performance win over the default reference behavior;
2. the corrected Scenario C reruns (`017-020`) showed that explicit targeted-standby execution can now work repeatedly after the ext-address retention fix.

The next sensible step is to first rerun the full Milestone 8 scenario set on the current codebase so the starting point for Milestone 9 is freshly measured. After that baseline refresh, the milestone can proceed with smaller improvements aimed at the remaining practical problems:

- too much detach/reattach churn,
- too-frequent generic fallback after preferred attempts,
- insufficiently precise instrumentation for why fallback still occurred.

## Scope

In scope:

- small, reviewable controller/policy adjustments,
- targeted-attach stability improvements,
- fallback-path tightening,
- improved diagnostics for preferred success/miss/timeout and fallback cause,
- limited repeat validation on the most relevant hardware scenarios.

Out of scope:

- a full architecture rewrite,
- claiming a final performance win before new evidence exists,
- broad release packaging work.

## Initial priorities

### 1. Full rerun of the Milestone 8 scenario set

Before tuning the policy further, run the full hardware scenario set defined in Milestone 8 against the current implementation.

This first step should include the same comparison structure used for Milestone 8, covering at minimum:

- nominal reference vs biparental runs,
- downlink-only runs,
- uplink-only runs,
- symmetric runs,
- Scenario C active-parent ramp-down runs.

The purpose of this rerun is to establish an updated baseline for Milestone 9 work, confirm whether the post-Milestone-8 fixes materially changed the comparison picture, and identify the most productive next tuning target from fresh evidence instead of older batches alone.

Expected outputs for this first step:

- raw logs for every scenario/variant run,
- refreshed generated summaries,
- a short written checkpoint summarizing whether the current code still reproduces the Milestone 8 conclusions or has already shifted materially.

### Checkpoint — Scenario C-focused rerun (2026-05-05)

Although Milestone 9 initially proposed rerunning the full Milestone 8 matrix first, the immediate follow-up was narrowed to **Scenario C only** so the next decisions could focus on the most promising path: explicit targeted standby switching under active-parent degradation.

New Scenario C artifacts added in this checkpoint:

- `artifacts/milestone-8/active-parent-ramp-down/A/run-021-router-ramp-ed-8dB.log`
- `artifacts/milestone-8/active-parent-ramp-down/A/run-022-router-ramp-ed-8dB.log`
- `artifacts/milestone-8/active-parent-ramp-down/B/run-021-router-ramp-ed-8dB.log`
- `artifacts/milestone-8/active-parent-ramp-down/B/run-022-router-ramp-ed-8dB.log`
- regenerated `artifacts/milestone-8/generated/trial-summary.csv`
- regenerated `artifacts/milestone-8/generated/trial-summary.md`

Short result summary:

| Run set | Key result |
|---|---|
| Variant A `021-022` | both runs remained stable (`1` attach, `0` detaches each) |
| Variant B `021` | `8` attaches / `7` detaches; targeted `5 success / 0 miss / 0 timeout`; generic fallback `2` |
| Variant B `022` | `8` attaches / `7` detaches; targeted `3 success / 2 miss / 0 timeout`; generic fallback `3` |

Extended post-fix Scenario C picture using Variant B runs `017-022`:

- targeted outcomes total: `29 success / 12 miss / 0 timeout`
- average detaches per run: `7.5`
- average generic fallback uses per run: `1.7`
- average trigger-to-stable window: about `3879 ms`

Interpretation:

- the targeted standby path continues to work repeatedly after the ext-address retention fix;
- no new evidence suggests a return to the older `target_ext=unknown` / missing preferred identity failure mode;
- however, the practical problem remains unchanged: Variant B still shows heavy detach/reattach churn and still escalates to generic fallback too often to call this a clean win.

Milestone 9 implication:

- the next engineering focus should stay on reducing generic fallback and post-switch churn within Scenario C before investing in a broader renewed A/B comparison sweep.

### 2. Reduce unnecessary generic fallback

Investigate why generic fallback is still triggered after targeted standby requests, especially when the targeted path is already making progress.

Focus areas:

- whether preferred attempts are being judged too early,
- whether parent identity confirmation is lagging the actual attach outcome,
- whether generic fallback is being triggered in cases that should remain in a preferred/stabilizing path longer.

### 3. Reduce post-switch churn

Investigate why Variant B still shows repeated detach/reattach cycles even in runs where targeted standby succeeds.

Focus areas:

- behavior immediately after preferred success,
- interactions between `preferred_reattach_timeout`, `post_failover_stabilize_time`, and `hold_down_time`,
- whether recovery logic is re-entering too aggressively after a successful transition.

### 4. Improve observability for follow-up tuning

Add or refine diagnostics so each fallback and post-failover transition is easier to explain from logs alone.

Desired evidence improvements:

- explicit fallback cause logging,
- clearer distinction between "targeted attach did not start", "started but missed", and "started but timed out",
- better visibility into what parent the node attached to immediately before fallback decisions.

### 5. Re-validate narrowly before expanding scope

After each meaningful tuning change, prefer a narrow validation batch before any larger sweep.

Recommended first validation focus:

- Scenario C in-run ramp,
- nominal Variant B sanity,
- comparison against Variant A only where needed to confirm that an observed effect is specific to the biparental path.

## Success criteria for this milestone

Milestone 9 should be considered successful if it produces a clearly improved policy revision that shows one or more of the following under repeatable hardware testing:

- fewer generic fallback events,
- fewer detach/reattach cycles,
- more consistent targeted standby success,
- clearer diagnostics that make remaining failures directly actionable.

This milestone may still end without a full performance-win claim; a worthwhile outcome is a cleaner, more stable candidate for the next comparison round.

## Planned deliverables

- incremental code and configuration adjustments,
- updated milestone notes as improvements are tested,
- refreshed evidence in `artifacts/milestone-8/` or later follow-up artifact locations as appropriate,
- a short closeout summary stating which incremental changes helped and which did not.
