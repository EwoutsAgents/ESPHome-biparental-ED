#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 6 ]]; then
  cat >&2 <<'USAGE'
Usage: scripts/milestone8-log-trial.sh <variant> <scenario> <run_id> <router_power> <ed_power> <yaml> [device] [active_parent_id] [active_parent_tx] [standby_router_tx]

Captures a timestamped ESPHome log for one approved Milestone 8 trial.
This script does not flash firmware. Build/flash device images separately before trial collection.

Example:
  scripts/milestone8-log-trial.sh A downlink 001 -4dB 8dB examples/milestone-8-variant-a-reference-ed.yaml /dev/ttyACM0
USAGE
  exit 2
fi

variant="$1"
scenario="$2"
run_id="$3"
router_power="$4"
ed_power="$5"
yaml="$6"
device="${7:-}"
active_parent_id="${8:-unknown}"
active_parent_tx="${9:-${router_power}}"
standby_router_tx="${10:-${router_power}}"
active_parent_tx_mode="${ACTIVE_PARENT_TX_MODE:-}"
active_parent_tx_schedule="${ACTIVE_PARENT_TX_SCHEDULE:-}"
active_parent_step_duration_seconds="${ACTIVE_PARENT_STEP_DURATION_SECONDS:-}"
active_parent_upload_overhead_seconds="${ACTIVE_PARENT_UPLOAD_OVERHEAD_SECONDS:-}"
duration_seconds="${DURATION_SECONDS:-}"

ESPHOME_BIN="${ESPHOME_BIN:-esphome}"
root="$(cd "$(dirname "$0")/.." && pwd)"
out_dir="${root}/artifacts/milestone-8/${scenario}/${variant}"
mkdir -p "${out_dir}"
stem="run-${run_id}-router-${router_power}-ed-${ed_power}"
meta="${out_dir}/${stem}.meta.tsv"
log="${out_dir}/${stem}.log"

{
  printf 'variant\t%s\n' "${variant}"
  printf 'scenario\t%s\n' "${scenario}"
  printf 'run_id\t%s\n' "${run_id}"
  printf 'router_output_power\t%s\n' "${router_power}"
  printf 'ed_output_power\t%s\n' "${ed_power}"
  printf 'active_parent_router_id\t%s\n' "${active_parent_id}"
  printf 'active_parent_tx_power\t%s\n' "${active_parent_tx}"
  printf 'standby_router_tx_power\t%s\n' "${standby_router_tx}"
  if [[ -n "${active_parent_tx_mode}" ]]; then
    printf 'active_parent_tx_mode\t%s\n' "${active_parent_tx_mode}"
  fi
  if [[ -n "${active_parent_tx_schedule}" ]]; then
    printf 'active_parent_tx_schedule\t%s\n' "${active_parent_tx_schedule}"
  fi
  if [[ -n "${active_parent_step_duration_seconds}" ]]; then
    printf 'active_parent_step_duration_seconds\t%s\n' "${active_parent_step_duration_seconds}"
  fi
  if [[ -n "${active_parent_upload_overhead_seconds}" ]]; then
    printf 'active_parent_upload_overhead_seconds\t%s\n' "${active_parent_upload_overhead_seconds}"
  fi
  if [[ -n "${duration_seconds}" ]]; then
    printf 'duration_seconds\t%s\n' "${duration_seconds}"
  fi
  printf 'yaml\t%s\n' "${yaml}"
  printf 'started_utc\t%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
} | tee "${meta}"

echo "Writing log to ${log}" >&2
if [[ -n "${device}" ]]; then
  exec "${ESPHOME_BIN}" logs "${yaml}" --device "${device}" 2>&1 | tee "${log}"
else
  exec "${ESPHOME_BIN}" logs "${yaml}" 2>&1 | tee "${log}"
fi
