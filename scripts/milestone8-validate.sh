#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/../examples"

ESPHOME_BIN="${ESPHOME_BIN:-esphome}"

configs=(
  milestone-8-variant-a-reference-ed.yaml
  milestone-8-variant-b-biparental-ed.yaml
  link-degradation-router.yaml
  milestone-8-scenario-c-active-parent-router.yaml
  milestone-8-scenario-c-standby-router.yaml
)

for cfg in "${configs[@]}"; do
  echo "== esphome config ${cfg}"
  "${ESPHOME_BIN}" config "${cfg}"
done

if [[ "${1:-}" == "--compile" ]]; then
  for cfg in "${configs[@]}"; do
    echo "== esphome compile ${cfg}"
    "${ESPHOME_BIN}" compile "${cfg}"
  done
else
  echo "Config validation complete. Pass --compile to also build firmware images."
fi
