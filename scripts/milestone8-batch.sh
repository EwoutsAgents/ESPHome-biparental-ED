#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

ESPHOME_BIN="${ESPHOME_BIN:-esphome}"
ED_DEVICE="${ED_DEVICE:-/dev/ttyACM0}"
ROUTER_DEVICES_CSV="${ROUTER_DEVICES_CSV:-/dev/ttyACM2,/dev/ttyACM3,/dev/ttyACM4}"
REPEATS="${REPEATS:-2}"
DURATION_SECONDS="${DURATION_SECONDS:-180}"
RESTORE_NOMINAL_AT_END="${RESTORE_NOMINAL_AT_END:-1}"

A_YAML="examples/milestone-8-variant-a-reference-ed.yaml"
B_YAML="examples/milestone-8-variant-b-biparental-ed.yaml"
ROUTER_YAML="examples/link-degradation-router.yaml"

SCENARIOS=(
  "nominal-callback-baseline|8dB|8dB"
  "downlink-only|-4dB|8dB"
  "uplink-only|8dB|-4dB"
  "symmetric|-4dB|-4dB"
)

IFS=',' read -r -a ROUTER_DEVICES <<< "$ROUTER_DEVICES_CSV"

declare -A BUILT_KEYS=()
CURRENT_ROUTER_POWER=""

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
  power="${power//- /}"
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

build_dir_for_yaml() {
  local yaml="$1"
  case "$yaml" in
    "$A_YAML") echo "$ROOT/examples/.esphome/build/m8-reference-ed" ;;
    "$B_YAML") echo "$ROOT/examples/.esphome/build/m8-biparental-ed" ;;
    "$ROUTER_YAML") echo "$ROOT/examples/.esphome/build/ot-router-degrade" ;;
    *) return 1 ;;
  esac
}

build_config() {
  local yaml="$1"
  local power="$2"
  local key="${yaml}|${power}"
  local build_dir materialized_yaml
  if [[ -n "${BUILT_KEYS[$key]:-}" ]]; then
    return 0
  fi
  build_dir="$(build_dir_for_yaml "$yaml")"
  if [[ -n "$build_dir" && -d "$build_dir" ]]; then
    log "Removing stale build dir $build_dir"
    rm -rf "$build_dir"
  fi
  materialized_yaml="$(materialize_yaml "$yaml" "$power")"
  run_esphome "$ESPHOME_BIN" compile "$materialized_yaml"
  BUILT_KEYS[$key]=1
}

upload_config() {
  local yaml="$1"
  local power="$2"
  local device="$3"
  local materialized_yaml
  materialized_yaml="$(materialize_yaml "$yaml" "$power")"
  run_esphome "$ESPHOME_BIN" upload "$materialized_yaml" --device "$device"
}

ensure_router_power() {
  local power="$1"
  if [[ "$CURRENT_ROUTER_POWER" == "$power" ]]; then
    return 0
  fi
  build_config "$ROUTER_YAML" "$power"
  for dev in "${ROUTER_DEVICES[@]}"; do
    upload_config "$ROUTER_YAML" "$power" "$dev"
  done
  CURRENT_ROUTER_POWER="$power"
}

next_run_id() {
  local scenario="$1"
  local variant="$2"
  local dir="$ROOT/artifacts/milestone-8/$scenario/$variant"
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

log_trial() {
  local variant="$1"
  local scenario="$2"
  local router_power="$3"
  local ed_power="$4"
  local yaml="$5"
  local run_id="$6"
  local out_dir="$ROOT/artifacts/milestone-8/$scenario/$variant"
  local stem="run-${run_id}-router-${router_power}-ed-${ed_power}"
  local meta="$out_dir/${stem}.meta.tsv"
  local log_file="$out_dir/${stem}.log"
  mkdir -p "$out_dir"

  {
    printf 'variant\t%s\n' "$variant"
    printf 'scenario\t%s\n' "$scenario"
    printf 'run_id\t%s\n' "$run_id"
    printf 'router_output_power\t%s\n' "$router_power"
    printf 'ed_output_power\t%s\n' "$ed_power"
    printf 'yaml\t%s\n' "$yaml"
    printf 'duration_seconds\t%s\n' "$DURATION_SECONDS"
    printf 'started_utc\t%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  } > "$meta"

  log "Capturing ${variant} ${scenario} run ${run_id} for ${DURATION_SECONDS}s -> ${log_file}"
  set +e
  local materialized_yaml
  materialized_yaml="$(materialize_yaml "$yaml" "$ed_power")"
  timeout --signal=INT "${DURATION_SECONDS}s" "$ESPHOME_BIN" logs "$materialized_yaml" --device "$ED_DEVICE" 2>&1 | tee "$log_file"
  local cmd_status=${PIPESTATUS[0]}
  set -e
  if [[ $cmd_status -ne 0 && $cmd_status -ne 124 && $cmd_status -ne 130 ]]; then
    log "Log capture failed with status ${cmd_status}"
    return "$cmd_status"
  fi
  printf 'ended_utc\t%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "$meta"
}

run_variant_trial() {
  local variant="$1"
  local scenario="$2"
  local router_power="$3"
  local ed_power="$4"
  local yaml="$5"
  local run_id
  run_id="$(next_run_id "$scenario" "$variant")"
  build_config "$yaml" "$ed_power"
  upload_config "$yaml" "$ed_power" "$ED_DEVICE"
  sleep 2
  log_trial "$variant" "$scenario" "$router_power" "$ed_power" "$yaml" "$run_id"
}

main() {
  log "Milestone 8 batch starting: repeats=$REPEATS duration=${DURATION_SECONDS}s ed=$ED_DEVICE routers=$ROUTER_DEVICES_CSV"
  for scenario_spec in "${SCENARIOS[@]}"; do
    IFS='|' read -r scenario router_power ed_power <<< "$scenario_spec"
    log "Scenario $scenario router=$router_power ed=$ed_power"
    ensure_router_power "$router_power"
    for ((i=1; i<=REPEATS; i++)); do
      log "Scenario $scenario repeat $i/$REPEATS variant A"
      run_variant_trial "A" "$scenario" "$router_power" "$ed_power" "$A_YAML"
      log "Scenario $scenario repeat $i/$REPEATS variant B"
      run_variant_trial "B" "$scenario" "$router_power" "$ed_power" "$B_YAML"
    done
  done

  python3 scripts/milestone8-summarize.py

  if [[ "$RESTORE_NOMINAL_AT_END" == "1" ]]; then
    log "Restoring routers to nominal 8dB"
    CURRENT_ROUTER_POWER=""
    ensure_router_power "8dB"
    log "Restoring ED to nominal Variant B 8dB"
    build_config "$B_YAML" "8dB"
    upload_config "$B_YAML" "8dB" "$ED_DEVICE"
  fi

  log "Milestone 8 batch complete"
}

main "$@"
