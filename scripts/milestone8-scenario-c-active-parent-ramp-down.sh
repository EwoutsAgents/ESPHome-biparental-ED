#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

ESPHOME_BIN="${ESPHOME_BIN:-esphome}"
ED_DEVICE="${ED_DEVICE:-/dev/ttyACM0}"
ACTIVE_PARENT_DEVICE="${ACTIVE_PARENT_DEVICE:-/dev/ttyACM2}"
STANDBY_ROUTER_DEVICES_CSV="${STANDBY_ROUTER_DEVICES_CSV:-/dev/ttyACM3,/dev/ttyACM4}"
REPEATS="${REPEATS:-2}"
ACTIVE_PARENT_ID="${ACTIVE_PARENT_ID:-P1}"
RESTORE_NOMINAL_AT_END="${RESTORE_NOMINAL_AT_END:-1}"

A_YAML="examples/milestone-8-variant-a-reference-ed.yaml"
B_YAML="examples/milestone-8-variant-b-biparental-ed.yaml"
ACTIVE_ROUTER_YAML="examples/milestone-8-scenario-c-active-parent-router.yaml"
ACTIVE_ROUTER_OFF_YAML="examples/milestone-8-scenario-c-active-parent-router-off.yaml"
STANDBY_ROUTER_YAML="examples/milestone-8-scenario-c-standby-router.yaml"

TX_STEPS=("8dB" "4dB" "0dB" "-4dB" "-8dB" "-12dB" "-13dB" "-14dB" "-15dB" "off")
STEP_DURATION_SECONDS="${STEP_DURATION_SECONDS:-30}"
UPLOAD_OVERHEAD_SECONDS="${UPLOAD_OVERHEAD_SECONDS:-12}"
FINAL_MARGIN_SECONDS="${FINAL_MARGIN_SECONDS:-15}"
DURATION_SECONDS="${DURATION_SECONDS:-$((STEP_DURATION_SECONDS * ${#TX_STEPS[@]} + UPLOAD_OVERHEAD_SECONDS * (${#TX_STEPS[@]} - 1) + FINAL_MARGIN_SECONDS))}"
SCENARIO="active-parent-ramp-down"

IFS=',' read -r -a STANDBY_ROUTER_DEVICES <<< "$STANDBY_ROUTER_DEVICES_CSV"

declare -A BUILT_KEYS=()
CURRENT_ACTIVE_TX=""
CURRENT_STANDBY_TX=""
ACTIVE_TX_SCHEDULE="$(printf '%s -> ' "${TX_STEPS[@]}")"
ACTIVE_TX_SCHEDULE="${ACTIVE_TX_SCHEDULE% -> }"

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
  if [[ "$power" == "off" ]]; then
    cp "$ACTIVE_ROUTER_OFF_YAML" "$out"
    echo "$out"
    return 0
  fi
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

upload_config_quiet() {
  local yaml="$1"
  local power="$2"
  local device="$3"
  local log_file="$4"
  local materialized
  materialized="$(materialize_yaml "$yaml" "$power")"
  {
    log "$ESPHOME_BIN upload $materialized --device $device"
    "$ESPHOME_BIN" upload "$materialized" --device "$device"
  } >> "$log_file" 2>&1
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
  local yaml="$ACTIVE_ROUTER_YAML"
  if [[ "$CURRENT_ACTIVE_TX" == "$tx" ]]; then
    return 0
  fi
  if [[ "$tx" == "off" ]]; then
    yaml="$ACTIVE_ROUTER_OFF_YAML"
  fi
  build_config "$yaml" "$tx"
  upload_config "$yaml" "$tx" "$ACTIVE_PARENT_DEVICE"
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
  local yaml="$2"
  local run_id ramp_log ramp_pid status
  run_id="$(next_run_id "$variant")"
  ramp_log="$ROOT/artifacts/milestone-8/$SCENARIO/$variant/run-${run_id}-active-parent-ramp.log"
  mkdir -p "$(dirname "$ramp_log")"

  build_config "$yaml" "8dB"
  upload_config "$yaml" "8dB" "$ED_DEVICE"
  set_active_parent_power "${TX_STEPS[0]}"
  sleep 2

  : > "$ramp_log"
  (
    for ((idx=1; idx<${#TX_STEPS[@]}; idx++)); do
      sleep "$STEP_DURATION_SECONDS"
      local tx="${TX_STEPS[$idx]}"
      local yaml="$ACTIVE_ROUTER_YAML"
      if [[ "$tx" == "off" ]]; then
        yaml="$ACTIVE_ROUTER_OFF_YAML"
      fi
      log "Scenario C run ${run_id}: switching active parent TX to ${tx}"
      upload_config_quiet "$yaml" "$tx" "$ACTIVE_PARENT_DEVICE" "$ramp_log"
    done
  ) >> "$ramp_log" 2>&1 &
  ramp_pid=$!

  log "Capture ${SCENARIO} variant=${variant} run=${run_id} active=ramp standby=8dB ed=8dB schedule=${ACTIVE_TX_SCHEDULE}"
  set +e
  ACTIVE_PARENT_TX_MODE="in_run_ramp" \
  ACTIVE_PARENT_TX_SCHEDULE="$ACTIVE_TX_SCHEDULE" \
  ACTIVE_PARENT_STEP_DURATION_SECONDS="$STEP_DURATION_SECONDS" \
  ACTIVE_PARENT_UPLOAD_OVERHEAD_SECONDS="$UPLOAD_OVERHEAD_SECONDS" \
  DURATION_SECONDS="$DURATION_SECONDS" \
  timeout --signal=INT "${DURATION_SECONDS}s" \
    scripts/milestone8-log-trial.sh \
      "$variant" "$SCENARIO" "$run_id" "ramp" "8dB" "$yaml" "$ED_DEVICE" \
      "$ACTIVE_PARENT_ID" "$ACTIVE_TX_SCHEDULE" "8dB"
  status=$?
  set -e

  if kill -0 "$ramp_pid" 2>/dev/null; then
    kill "$ramp_pid" 2>/dev/null || true
  fi
  wait "$ramp_pid" 2>/dev/null || true
  CURRENT_ACTIVE_TX=""

  if [[ $status -ne 0 && $status -ne 124 && $status -ne 130 ]]; then
    return "$status"
  fi
}

main() {
  local minimum_duration_seconds
  if (( STEP_DURATION_SECONDS < 1 )); then
    echo "STEP_DURATION_SECONDS must be >= 1" >&2
    exit 2
  fi
  if (( UPLOAD_OVERHEAD_SECONDS < 0 )); then
    echo "UPLOAD_OVERHEAD_SECONDS must be >= 0" >&2
    exit 2
  fi
  if (( FINAL_MARGIN_SECONDS < 0 )); then
    echo "FINAL_MARGIN_SECONDS must be >= 0" >&2
    exit 2
  fi
  minimum_duration_seconds=$((STEP_DURATION_SECONDS * ${#TX_STEPS[@]} + UPLOAD_OVERHEAD_SECONDS * (${#TX_STEPS[@]} - 1) + FINAL_MARGIN_SECONDS))
  if (( DURATION_SECONDS < minimum_duration_seconds )); then
    echo "DURATION_SECONDS must cover all TX steps plus upload overhead (need at least ${minimum_duration_seconds}s)" >&2
    exit 2
  fi

  log "Scenario C start: repeats=$REPEATS duration=${DURATION_SECONDS}s step_duration=${STEP_DURATION_SECONDS}s upload_overhead=${UPLOAD_OVERHEAD_SECONDS}s final_margin=${FINAL_MARGIN_SECONDS}s ed=$ED_DEVICE active_parent=$ACTIVE_PARENT_DEVICE standby=$STANDBY_ROUTER_DEVICES_CSV schedule=${ACTIVE_TX_SCHEDULE}"
  ensure_standby_nominal

  for tx in "${TX_STEPS[@]}"; do
    if [[ "$tx" == "off" ]]; then
      build_config "$ACTIVE_ROUTER_OFF_YAML" "$tx"
    else
      build_config "$ACTIVE_ROUTER_YAML" "$tx"
    fi
  done

  for ((i=1; i<=REPEATS; i++)); do
    log "Scenario C repeat $i/$REPEATS variant A"
    run_trial "A" "$A_YAML"
    log "Scenario C repeat $i/$REPEATS variant B"
    run_trial "B" "$B_YAML"
  done

  python3 scripts/milestone8-summarize.py

  if [[ "$RESTORE_NOMINAL_AT_END" == "1" ]]; then
    CURRENT_ACTIVE_TX=""
    set_active_parent_power "8dB"
    ensure_standby_nominal
    upload_config "$B_YAML" "8dB" "$ED_DEVICE"
  fi

  log "Scenario C complete"
}

main "$@"
