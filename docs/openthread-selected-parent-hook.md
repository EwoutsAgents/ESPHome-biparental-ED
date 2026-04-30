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
- OpenThread debug lines from the added instrumentation:
  - `SelectedParent ParentResponse rx src=...`
  - `SelectedParent ChildIdRequest sent cand=...`
  - `SelectedParent ChildIdResponse accepted parent=... child=...`
  - `SelectedParent ParentResponse reject err=...` (only on failure)
  - `SelectedParent ChildIdRequest timed out cand=...` (failure path)

If the hook is absent, logs show:

- `Targeted standby attach unavailable: missing OpenThread selected-parent hook ...`

and fallback generic reattach is used.

## Milestone-8 root cause and fix summary

- **Root cause**: selected-parent attach could receive/accept Parent Response but still never send ChildIdRequest.
- **Why**: `HasAcceptableParentCandidate()` required `mReceivedResponseFromParent` for `kSelectedParent` while already attached as child, causing candidate gating to fail in this mode.
- **Fix**: bypass that `mReceivedResponseFromParent` gate specifically for `kSelectedParent` (keep it for other attach modes).

## Evidence

- Pre-fix debug run showed `SelectedParent ParentResponse accepted ...` followed by timer state with `has_candidate=0` and eventual ChildId timeout.
- Post-fix run `artifacts/milestone-8/targeted-hook-smoke/run-011-selected-parent-gate-bypass.log` shows:
  - ParentResponse accepted,
  - ChildIdRequest sent in selected-parent mode,
  - ChildIdResponse accepted,
  - targeted standby outcome success.
