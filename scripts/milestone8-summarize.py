#!/usr/bin/env python3
from __future__ import annotations

import csv
import re
from dataclasses import dataclass, asdict
from pathlib import Path
from statistics import median
from typing import Iterable

ROOT = Path(__file__).resolve().parent.parent
ARTIFACTS = ROOT / "artifacts" / "milestone-8"

TS_RE = re.compile(r"^\[(\d\d):(\d\d):(\d\d)\.(\d\d\d)\]")
DIAG_RE = re.compile(r"standby=(yes|no).+failovers=(\d+).+preferred\(a/s/m\)=(\d+)/(\d+)/(\d+).+outcome=([a-z]+)")


@dataclass
class TrialSummary:
    scenario: str
    variant: str
    run: str
    router_power: str
    ed_power: str
    log_path: str
    active_parent_router_id: str = ""
    active_parent_tx_power: str = ""
    standby_router_tx_power: str = ""
    output_power_dbm: str = ""
    attach_count: int = 0
    detach_count: int = 0
    detach_to_child_windows: int = 0
    detach_to_child_median_ms: str = ""
    detach_to_child_max_ms: str = ""
    initial_attach_ms: str = ""
    standby_yes: int = 0
    standby_no: int = 0
    diag_lines: int = 0
    callback_registered: int = 0
    preferred_requests: int = 0
    generic_requests: int = 0
    parent_response_security_warnings: int = 0
    preferred_success: int = 0
    preferred_miss: int = 0
    preferred_timeout: int = 0
    final_state: str = ""
    final_outcome: str = ""
    final_failovers: str = ""
    # Scenario-C / variant-focused metrics
    standby_discovered: str = ""
    targeted_attach_requested: int = 0
    targeted_outcome_success: int = 0
    targeted_outcome_miss: int = 0
    targeted_outcome_timeout: int = 0
    generic_fallback_used: int = 0
    failover_trigger_to_stable_child_ms: str = ""
    final_parent_after_recovery: str = ""
    natural_parent_switch: str = ""


def parse_ts_ms(line: str) -> int | None:
    m = TS_RE.match(line)
    if not m:
        return None
    hh, mm, ss, ms = map(int, m.groups())
    return (((hh * 60) + mm) * 60 + ss) * 1000 + ms


def parse_meta(meta_path: Path) -> dict[str, str]:
    out: dict[str, str] = {}
    for raw in meta_path.read_text().splitlines():
        if not raw.strip() or "\t" not in raw:
            continue
        k, v = raw.split("\t", 1)
        out[k] = v
    return out


def iter_trial_logs() -> Iterable[tuple[Path, Path]]:
    for log_path in sorted(ARTIFACTS.glob("*/*/run-*.log")):
        meta_path = log_path.with_suffix(".meta.tsv")
        if meta_path.exists():
            yield log_path, meta_path


def summarize(log_path: Path, meta_path: Path) -> TrialSummary:
    meta = parse_meta(meta_path)
    summary = TrialSummary(
        scenario=meta.get("scenario", log_path.parent.parent.name),
        variant=meta.get("variant", log_path.parent.name),
        run=meta.get("run_id", log_path.stem),
        router_power=meta.get("router_output_power", ""),
        ed_power=meta.get("ed_output_power", ""),
        active_parent_router_id=meta.get("active_parent_router_id", ""),
        active_parent_tx_power=meta.get("active_parent_tx_power", ""),
        standby_router_tx_power=meta.get("standby_router_tx_power", ""),
        log_path=str(log_path.relative_to(ROOT)),
    )

    first_ts = None
    pending_detach_ts = None
    failover_trigger_ts = None
    durations: list[int] = []
    last_diag = None
    last_rloc = None
    natural_parent_switch = False

    for line in log_path.read_text(errors="replace").splitlines():
        ts = parse_ts_ms(line)
        if ts is not None and first_ts is None:
            first_ts = ts

        if "Output power:" in line and not summary.output_power_dbm:
            summary.output_power_dbm = line.rsplit("Output power:", 1)[1].strip()

        if "Registered OpenThread parent response callback" in line:
            summary.callback_registered += 1
        if "Requesting preferred failover" in line:
            summary.preferred_requests += 1
            if failover_trigger_ts is None and ts is not None:
                failover_trigger_ts = ts
        if "Requesting targeted standby attach" in line:
            summary.targeted_attach_requested += 1
            if failover_trigger_ts is None and ts is not None:
                failover_trigger_ts = ts
        if "Targeted standby outcome=success" in line:
            summary.targeted_outcome_success += 1
        if "Targeted standby outcome=miss" in line:
            summary.targeted_outcome_miss += 1
        if "Targeted standby outcome=timeout" in line:
            summary.targeted_outcome_timeout += 1
        if "Requesting generic reattach/failover" in line:
            summary.generic_requests += 1
            summary.generic_fallback_used += 1
            if failover_trigger_ts is None and ts is not None:
                failover_trigger_ts = ts
        if "Failed to process Parent Response: Security" in line:
            summary.parent_response_security_warnings += 1
        if "Role child -> detached" in line:
            summary.detach_count += 1
            pending_detach_ts = ts
            if failover_trigger_ts is None and ts is not None:
                failover_trigger_ts = ts
        if "Role detached -> child" in line:
            summary.attach_count += 1
            if first_ts is not None and summary.initial_attach_ms == "":
                summary.initial_attach_ms = str(ts - first_ts)
            if pending_detach_ts is not None and ts is not None and ts >= pending_detach_ts:
                durations.append(ts - pending_detach_ts)
                pending_detach_ts = None
            if failover_trigger_ts is not None and ts is not None and ts >= failover_trigger_ts and summary.failover_trigger_to_stable_child_ms == "":
                summary.failover_trigger_to_stable_child_ms = str(ts - failover_trigger_ts)
        if "RLOC16 " in line and " -> " in line:
            m = re.search(r"RLOC16\s+([0-9a-fA-F]{4})\s+->\s+([0-9a-fA-F]{4})", line)
            if m:
                frm = m.group(1).lower()
                to = m.group(2).lower()
                last_rloc = to
                if frm != "fffe" and to != "fffe" and frm != to:
                    natural_parent_switch = True
        if "[D][biparental_ed.diag:" in line:
            summary.diag_lines += 1
            if "standby=yes" in line:
                summary.standby_yes += 1
            if "standby=no" in line:
                summary.standby_no += 1
            last_diag = line

    if durations:
        summary.detach_to_child_windows = len(durations)
        summary.detach_to_child_median_ms = str(int(median(durations)))
        summary.detach_to_child_max_ms = str(max(durations))

    if last_diag:
        m = DIAG_RE.search(last_diag)
        if m:
            standby, failovers, pref_a, pref_s, pref_m, outcome = m.groups()
            summary.preferred_success = int(pref_s)
            summary.preferred_miss = int(pref_m)
            summary.preferred_timeout = max(0, int(pref_a) - int(pref_s) - int(pref_m))
            state_match = re.search(r"state=(\d+)", last_diag)
            summary.final_state = state_match.group(1) if state_match else ""
            summary.final_outcome = outcome
            summary.final_failovers = failovers
            summary.final_parent_after_recovery = re.search(r"active=(0x[0-9a-fA-F]+)", last_diag).group(1) if re.search(r"active=(0x[0-9a-fA-F]+)", last_diag) else ""

    if not summary.final_parent_after_recovery and last_rloc and last_rloc != "fffe":
        summary.final_parent_after_recovery = f"0x{last_rloc}"

    if summary.variant == "B":
        summary.standby_discovered = "yes" if summary.standby_yes > 0 else "no"
    summary.natural_parent_switch = "yes" if natural_parent_switch else "no"
    return summary


def write_csv(rows: list[TrialSummary], path: Path) -> None:
    with path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(asdict(rows[0]).keys()))
        writer.writeheader()
        for row in rows:
            writer.writerow(asdict(row))


def write_markdown(rows: list[TrialSummary], path: Path) -> None:
    lines = [
        "# Milestone 8 generated trial summary",
        "",
        f"Generated from `{ARTIFACTS.relative_to(ROOT)}`.",
        "",
        "| Scenario | Variant | Run | Router | ED | ActiveParent | Active TX | Standby TX | Attach | Detach | Trigger→stable ms | Standby yes/no | Targeted req | Targeted s/m/t | Generic fallback | Preferred req | Final parent | Natural switch | Log |",
        "|---|---|---:|---:|---:|---|---:|---:|---:|---:|---:|---:|---:|---|---:|---:|---|---|---|",
    ]
    for row in rows:
        lines.append(
            f"| {row.scenario} | {row.variant} | {row.run} | {row.router_power} | {row.ed_power} | {row.active_parent_router_id or '-'} | {row.active_parent_tx_power or '-'} | {row.standby_router_tx_power or '-'} | {row.attach_count} | {row.detach_count} | {row.failover_trigger_to_stable_child_ms or '-'} | {row.standby_yes}/{row.standby_no} | {row.targeted_attach_requested} | {row.targeted_outcome_success}/{row.targeted_outcome_miss}/{row.targeted_outcome_timeout} | {row.generic_fallback_used} | {row.preferred_requests} | {row.final_parent_after_recovery or '-'} | {row.natural_parent_switch or '-'} | `{row.log_path}` |"
        )

    lines += [
        "",
        "## Coverage snapshot",
        "",
    ]

    scenarios = sorted({row.scenario for row in rows})
    variants = sorted({row.variant for row in rows})
    lines += ["| Scenario | " + " | ".join(variants) + " |", "|---|" + "|".join(["---"] * len(variants)) + "|"]
    for scenario in scenarios:
        counts = []
        for variant in variants:
            n = sum(1 for row in rows if row.scenario == scenario and row.variant == variant)
            counts.append(str(n) if n else "-")
        lines.append("| " + scenario + " | " + " | ".join(counts) + " |")

    path.write_text("\n".join(lines) + "\n")


def main() -> int:
    rows = [summarize(log_path, meta_path) for log_path, meta_path in iter_trial_logs()]
    if not rows:
        raise SystemExit("No trial logs found")
    out_dir = ARTIFACTS / "generated"
    out_dir.mkdir(parents=True, exist_ok=True)
    csv_path = out_dir / "trial-summary.csv"
    md_path = out_dir / "trial-summary.md"
    write_csv(rows, csv_path)
    write_markdown(rows, md_path)
    print(f"Wrote {csv_path.relative_to(ROOT)}")
    print(f"Wrote {md_path.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
