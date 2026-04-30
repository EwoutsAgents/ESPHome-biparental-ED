# OpenThread selected-parent hook for targeted standby attach

`biparental_ed` now requests targeted standby attach through:

- `biparental_ot_request_selected_parent_attach(otInstance *, const otExtAddress *)`

This symbol is intentionally outside public OpenThread API. It must be present in the OpenThread core build used by ESPHome.

## Why this exists

Public APIs do not expose "attach to this selected parent". `otThreadSearchForBetterParent()` is generic search and does not take a target parent selector.

## Hook patch scope

Patch points in the OpenThread source bundled by ESP-IDF:

- `src/core/thread/mle.hpp`
  - add `Mle::AttachToSelectedParent(const Mac::ExtAddress &)`
- `src/core/thread/mle.cpp`
  - implement `AttachToSelectedParent(...)`
  - in `Attacher::SendParentRequest(kToSelectedRouter)`, use selected ext address on MTD path instead of multicast.
- `src/core/api/thread_api.cpp`
  - export `extern "C" bool biparental_ot_request_selected_parent_attach(...)`.

## Apply helper script

Run:

```bash
python3 scripts/apply-openthread-selected-parent-hook.py
```

The script patches the local PlatformIO framework tree at:

`~/.platformio/packages/framework-espidf/components/openthread/openthread/src/core/`

## Verification

After patching, compile Milestone 8 variant B:

```bash
/home/ewout/.openclaw/workspace-softwaredeveloper/.venv-esphome/bin/esphome \
  compile examples/milestone-8-variant-b-biparental-ed.yaml
```

At runtime, targeted flow logs should include:

- `Requesting targeted standby attach target_rloc16=... target_ext=...`
- `Targeted standby attach accepted`

If the hook is absent, logs show:

- `Targeted standby attach unavailable: missing OpenThread selected-parent hook ...`

and fallback generic reattach is used.
