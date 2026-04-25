# Milestone 7 — Hardware Validation

Status: **PASS (closeout complete)**

## Closeout criteria (required timeout evidence)

Milestone 7 remained blocked until both markers were captured from a fresh hardware run:

- `Preferred outcome=timeout`
- `entity.last_failover_reason=preferred_timeout`

Both are now present in fresh logs from `/dev/ttyACM0`.

## Hardware + firmware used

- Repo: `/home/ewout/.openclaw/workspace-softwaredeveloper/biparental_ED/ESPHome-biparental-ED`
- Baseline code direction retained: continuity fallback in `mark_failover_complete()` (reported on `main` as `aa77706`)
- ED port: `/dev/ttyACM0`
- Router ports used by timeout forcing: `/dev/ttyACM2`, `/dev/ttyACM3`, `/dev/ttyACM4`
- Timeout-tuned config: `/tmp/m7-closeout-timeout.yaml`
  - `update_interval: 200ms`
  - `preferred_reattach_timeout: 300ms`
  - `runtime_debug_interval: 500ms`
  - `diagnostics_publish_interval: 1s`

Fresh image verification after flash:

- From `/tmp/m7-timeout-postflash.log`:
  - `[I][app:154]: ESPHome version 2026.4.1 compiled on 2026-04-25 19:51:34 +0200`

## Commands executed

Compile:

```bash
sg dialout -c '/home/ewout/.openclaw/workspace-softwaredeveloper/.venv-esphome/bin/esphome compile /tmp/m7-closeout-timeout.yaml'
```

Direct flash via esptool (resolved intermittent `esphome upload` connection failures on `/dev/ttyACM0`):

```bash
sg dialout -c '/home/ewout/.openclaw/workspace-softwaredeveloper/.venv-esphome/bin/esptool --before default-reset --after hard-reset --baud 115200 --port /dev/ttyACM0 --chip esp32c6 write-flash -z --flash-size detect 0x10000 /tmp/.esphome/build/biparental-ed-m7-closeout/.pioenvs/biparental-ed-m7-closeout/firmware.bin 0x0 /tmp/.esphome/build/biparental-ed-m7-closeout/.pioenvs/biparental-ed-m7-closeout/bootloader.bin 0x8000 /tmp/.esphome/build/biparental-ed-m7-closeout/.pioenvs/biparental-ed-m7-closeout/partitions.bin 0x9000 /tmp/.esphome/build/biparental-ed-m7-closeout/.pioenvs/biparental-ed-m7-closeout/ota_data_initial.bin'
```

Timeout-forcing hunt:

```bash
python3 /tmp/m7-timeout-prehold.py | tee /tmp/m7-final-timeout-prehold-v2.log
```

Entity capture pass:

```bash
timeout 40s sg dialout -c '/home/ewout/.openclaw/workspace-softwaredeveloper/.venv-esphome/bin/esphome logs /tmp/m7-closeout-timeout.yaml --device /dev/ttyACM0' | tee /tmp/m7-timeout-entity-capture.log
```

## Fresh required evidence

### 1) Preferred outcome classified as timeout

From `/tmp/m7-final-timeout-prehold-v2.log`:

- `[I][biparental_ed:317]: Preferred outcome=timeout target=0x5400 attached=0xffff -> result_state=7`

Also present in independent capture log:

- `/tmp/m7-timeout-entity-capture.log`
  - `[I][biparental_ed:317]: Preferred outcome=timeout target=0xe000 attached=0xffff -> result_state=7`

### 2) `last_failover_reason` captured as `preferred_timeout`

From `/tmp/m7-timeout-entity-capture.log`:

- `[D][main:406]: entity.last_failover_reason=preferred_timeout`

This line repeats across multiple successive samples in the same run, confirming durable publication of the timeout-classified failover reason.

## Result matrix

- Fresh `Preferred outcome=timeout`: **PASS**
- Fresh `entity.last_failover_reason=preferred_timeout`: **PASS**
- Milestone 7 timeout-specific closeout gate: **PASS**

## Artifacts

- `/tmp/m7-timeout-compile.log`
- `/tmp/m7-timeout-upload-ed.log`
- `/tmp/m7-timeout-postflash.log`
- `/tmp/m7-final-timeout-prehold-v2.log`
- `/tmp/m7-timeout-entity-capture.log`
- `/tmp/m7-closeout-timeout.yaml`
- `/tmp/m7-timeout-prehold.py`
