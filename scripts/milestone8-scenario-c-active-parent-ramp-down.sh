#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

ESPHOME_BIN="${ESPHOME_BIN:-esphome}"
ED_DEVICE="${ED_DEVICE:-/dev/ttyACM0}"
ACTIVE_PARENT_DEVICE="${ACTIVE_PARENT_DEVICE:-/dev/ttyACM2}"
STANDBY_ROUTER_DEVICES_CSV="${STANDBY_ROUTER_DEVICES_CSV:-/dev/ttyACM3,/dev/ttyACM4}"
REPEATS="${REPEATS:-2}"
DURATION_SECONDS="${DURATION_SECONDS:-180}"
ACTIVE_PARENT_ID="${ACTIVE_PARENT_ID:-P1}"
RESTORE_NOMINAL_AT_END="${RESTORE_NOMINAL_AT_END:-1}"

A_YAML="examples/milestone-8-variant-a-reference-ed.yaml"
B_YAML="examples/milestone-8-variant-b-biparental-ed.yaml"
ACTIVE_ROUTER_YAML="examples/milestone-8-scenario-c-active-parent-router.yaml"
STANDBY_ROUTER_YAML="examples/milestone-8-scenario-c-standby-router.yaml"

TX_STEPS=("8dB" "4dB" "0dB" "-4dB" "-8dB" "-12dB" "-15dB")
SCENARIO="active-parent-ramp-down"

IFS=',' read -r -a STANDBY_ROUTER_DEVICES <<< "$STANDBY_ROUTER_DEVICES_CSV"

declare -A BUILT_KEYS=()
CURRENT_ACTIVE_TX=""
CURRENT_STANDBY_TX=""

cleanup_temp_yamls() {
  rm -f "$ROOT"/examples/.m8tmp-*.yaml
}
trap cleanup_temp_yamls EXIT

log() {
  printf '[%s] %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$*"
}

run_esphome() {
  log "$*"
  "$@"
}

power_tag() {
  local power="$1"
  power="${power//-/neg}"
  power="${power//+/pos}"
  power="${power//./_}"
  echo "$power"
}

materialize_yaml() {
  local yaml="$1"
  local power="$2"
  local yaml_dir base stem out tag
  yaml_dir="$(dirname "$yaml")"
  base="$(basename "$yaml")"
  stem="${base%.yaml}"
  tag="$(power_tag "$power")"
  out="$yaml_dir/.m8tmp-${stem}-${tag}.yaml"
  python3 - "$yaml" "$out" "$power" <<'PY'
from pathlib import Path
import re
import sys
src = Path(sys.argv[1])
out = Path(sys.argv[2])
power = sys.argv[3]
text = src.read_text()
text, n = re.subn(r'(^\s*thread_output_power:\s*")[^"]+("\s*$)', rf'\g<1>{power}\2', text, count=1, flags=re.M)
if n != 1:
    raise SystemExit(f"Failed to rewrite thread_output_power in {src}")
out.write_text(text)
PY
  echo "$out"
}

build_key() {
  local yaml="$1"
  local power="$2"
  echo "${yaml}|${power}"
}

build_config() {
  local yaml="$1"
  local power="$2"
  local key
  key="$(build_key "$yaml" "$power")"
  if [[ -n "${BUILT_KEYS[$key]:-}" ]]; then
    return 0
  fi
  local materialized
  materialized="$(materialize_yaml "$yaml" "$power")"
  run_esphome "$ESPHOME_BIN" compile "$materialized"
  BUILT_KEYS[$key]=1
}

upload_config() {
  local yaml="$1"
  local power="$2"
  local device="$3"
  local materialized
  materialized="$(materialize_yaml "$yaml" "$power")"
  run_esphome "$ESPHOME_BIN" upload "$materialized" --device "$device"
}

ensure_standby_nominal() {
  local nominal="8dB"
  if [[ "$CURRENT_STANDBY_TX" == "$nominal" ]]; then
    return 0
  fi
  build_config "$STANDBY_ROUTER_YAML" "$nominal"
  for dev in "${STANDBY_ROUTER_DEVICES[@]}"; do
    upload_config "$STANDBY_ROUTER_YAML" "$nominal" "$dev"
  done
  CURRENT_STANDBY_TX="$nominal"
}

set_active_parent_power() {
  local tx="$1"
  if [[ "$CURRENT_ACTIVE_TX" == "$tx" ]]; then
    return 0
  fi
  build_config "$ACTIVE_ROUTER_YAML" "$tx"
  upload_config "$ACTIVE_ROUTER_YAML" "$tx" "$ACTIVE_PARENT_DEVICE"
  CURRENT_ACTIVE_TX="$tx"
}

next_run_id() {
  local variant="$1"
  local dir="$ROOT/artifacts/milestone-8/$SCENARIO/$variant"
  mkdir -p "$dir"
  local max=0
  shopt -s nullglob
  local file base num
  for file in "$dir"/run-*.log; do
    base="$(basename "$file")"
    num="${base#run-}"
    num="${num%%-*}"
    if [[ "$num" =~ ^[0-9]+$ ]] && (( 10#$num > max )); then
      max=$((10#$num))
    fi
  done
  shopt -u nullglob
  printf '%03d\n' $((max + 1))
}

run_trial() {
  local variant="$1"
  local active_tx="$2"
  local yaml="$3"
  local run_id
  run_id="$(next_run_id "$variant")"

  build_config "$yaml" "8dB"
  upload_config "$yaml" "8dB" "$ED_DEVICE"
  sleep 2

  log "Capture ${SCENARIO} variant=${variant} run=${run_id} active=${active_tx} standby=8dB ed=8dB"
  timeout --signal=INT "${DURATION_SECONDS}s" \
    scripts/milestone8-log-trial.sh \
      "$variant" "$SCENARIO" "$run_id" "$active_tx" "8dB" "$yaml" "$ED_DEVICE" \
      "$ACTIVE_PARENT_ID" "$active_tx" "8dB" || true
}

main() {
  log "Scenario C start: repeats=$REPEATS duration=${DURATION_SECONDS}s ed=$ED_DEVICE active_parent=$ACTIVE_PARENT_DEVICE standby=$STANDBY_ROUTER_DEVICES_CSV"
  ensure_standby_nominal

  for tx in "${TX_STEPS[@]}"; do
    set_active_parent_power "$tx"
    for ((i=1; i<=REPEATS; i++)); do
      log "TX step $tx repeat $i/$REPEATS variant A"
      run_trial "A" "$tx" "$A_YAML"
      log "TX step $tx repeat $i/$REPEATS variant B"
      run_trial "B" "$tx" "$B_YAML"
    done
  done

  python3 scripts/milestone8-summarize.py

  if [[ "$RESTORE_NOMINAL_AT_END" == "1" ]]; then
    set_active_parent_power "8dB"
    ensure_standby_nominal
    upload_config "$B_YAML" "8dB" "$ED_DEVICE"
  fi

  log "Scenario C complete"
}

main "$@"
