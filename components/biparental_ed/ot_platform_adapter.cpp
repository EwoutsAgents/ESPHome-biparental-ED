#include "ot_platform_adapter.h"

#include <optional>
#include <utility>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#if defined(USE_OPENTHREAD) || (__has_include("esphome/components/openthread/openthread.h") && __has_include(<openthread/thread.h>))
#define BIPARENTAL_ED_USE_OPENTHREAD
#include "esphome/components/openthread/openthread.h"
#include <openthread/thread.h>
#endif

namespace esphome {
namespace biparental_ed {

static const char *const TAG = "biparental_ed.ot";

#ifdef BIPARENTAL_ED_USE_OPENTHREAD
using esphome::openthread::InstanceLock;

static bool try_acquire_ot_lock(std::optional<InstanceLock> *lock, int delay_ms) {
  if (esphome::openthread::global_openthread_component == nullptr) {
    return false;
  }
  auto candidate = InstanceLock::try_acquire(delay_ms);
  if (!candidate) {
    return false;
  }
  *lock = std::move(candidate);
  return true;
}

static bool acquire_ot_lock_blocking(std::optional<InstanceLock> *lock) {
  if (esphome::openthread::global_openthread_component == nullptr) {
    return false;
  }
  if (!esphome::openthread::global_openthread_component->is_lock_initialized()) {
    return false;
  }
  *lock = InstanceLock::acquire();
  return true;
}

static bool fallback_connected_state() {
  if (esphome::openthread::global_openthread_component == nullptr) {
    return false;
  }
  return esphome::openthread::global_openthread_component->is_connected();
}
#endif

bool EspHomeOpenThreadPlatformAdapter::is_attached_as_child() const {
#ifdef BIPARENTAL_ED_USE_OPENTHREAD
  if (esphome::openthread::global_openthread_component == nullptr) {
    return false;
  }

  std::optional<InstanceLock> lock;
  if (!try_acquire_ot_lock(&lock, 100)) {
    if (!acquire_ot_lock_blocking(&lock)) {
      // Best-effort fallback: for MTD/MED, "connected" implies child role.
      static uint32_t last_fallback_log_ms = 0;
      const uint32_t now_ms = millis();
      if ((now_ms - last_fallback_log_ms) >= 5000) {
        ESP_LOGD(TAG, "is_attached_as_child: OT lock unavailable, fallback connected=%s",
                 fallback_connected_state() ? "true" : "false");
        last_fallback_log_ms = now_ms;
      }
      return fallback_connected_state();
    }
  }

  otInstance *instance = lock->get_instance();
  return otThreadGetDeviceRole(instance) == OT_DEVICE_ROLE_CHILD;
#else
  return false;
#endif
}

bool EspHomeOpenThreadPlatformAdapter::read_parent_metrics(ParentMetrics *metrics) {
  if (metrics == nullptr) {
    return false;
  }
#ifdef BIPARENTAL_ED_USE_OPENTHREAD
  auto log_throttled = [](const char *msg) {
    static uint32_t last_log_ms = 0;
    const uint32_t now_ms = millis();
    if ((now_ms - last_log_ms) >= 5000) {
      ESP_LOGD(TAG, "%s", msg);
      last_log_ms = now_ms;
    }
  };

  std::optional<InstanceLock> lock;
  if (!try_acquire_ot_lock(&lock, 100)) {
    if (!acquire_ot_lock_blocking(&lock)) {
      log_throttled("read_parent_metrics: OT lock unavailable");
      return false;
    }
  }

  otInstance *instance = lock->get_instance();

  const otDeviceRole role = otThreadGetDeviceRole(instance);
  if (role != OT_DEVICE_ROLE_CHILD) {
    log_throttled("read_parent_metrics: role is not CHILD yet");
    return false;
  }

  uint16_t parent_rloc16 = 0xffff;
  bool parent_info_ok = false;
  otRouterInfo parent_info{};
  const otError parent_info_err = otThreadGetParentInfo(instance, &parent_info);
  if (parent_info_err == OT_ERROR_NONE) {
    parent_info_ok = true;
    parent_rloc16 = parent_info.mRloc16;
  } else {
    static uint32_t last_parent_err_log_ms = 0;
    const uint32_t now_ms = millis();
    if ((now_ms - last_parent_err_log_ms) >= 5000) {
      ESP_LOGD(TAG, "read_parent_metrics: otThreadGetParentInfo err=%d", static_cast<int>(parent_info_err));
      last_parent_err_log_ms = now_ms;
    }
  }

  int8_t avg_rssi = -127;
  int8_t last_rssi = -127;
  (void) otThreadGetParentAverageRssi(instance, &avg_rssi);
  (void) otThreadGetParentLastRssi(instance, &last_rssi);

  // Find parent in neighbor table for age/link margin.
  // Fallback path: if otThreadGetParentInfo() failed, pick the freshest non-child neighbor.
  otNeighborInfoIterator it = OT_NEIGHBOR_INFO_ITERATOR_INIT;
  otNeighborInfo neighbor{};
  uint32_t parent_age_ms = 0;
  uint8_t parent_link_margin = 0;
  uint32_t best_fallback_age_s = UINT32_MAX;
  while (otThreadGetNextNeighborInfo(instance, &it, &neighbor) == OT_ERROR_NONE) {
    if (neighbor.mIsChild) {
      continue;
    }

    if (parent_info_ok) {
      if (neighbor.mRloc16 == parent_rloc16) {
        parent_age_ms = neighbor.mAge * 1000U;
        parent_link_margin = neighbor.mLinkMargin;
        break;
      }
    } else if (neighbor.mAge <= best_fallback_age_s) {
      best_fallback_age_s = neighbor.mAge;
      parent_rloc16 = neighbor.mRloc16;
      parent_age_ms = neighbor.mAge * 1000U;
      parent_link_margin = neighbor.mLinkMargin;
    }
  }

  if (parent_rloc16 == 0xffff) {
    log_throttled("read_parent_metrics: no parent RLOC found");
    return false;
  }

  metrics->valid = true;
  metrics->average_rssi = avg_rssi;
  metrics->last_rssi = last_rssi;
  metrics->parent_rloc16 = parent_rloc16;
  metrics->parent_link_margin = parent_link_margin;
  metrics->parent_age_ms = parent_age_ms;
  metrics->supervision_ok = true;
  metrics->control_plane_error_count = 0;

  // Approximate last parent RX timestamp from neighbor age.
  const uint32_t now_ms = millis();
  metrics->last_parent_rx_ms = now_ms - parent_age_ms;
  return true;
#else
  metrics->valid = false;
  return false;
#endif
}

bool EspHomeOpenThreadPlatformAdapter::read_router_neighbors(RouterNeighborCallback cb, void *context) {
  if (cb == nullptr) {
    return false;
  }
#ifdef BIPARENTAL_ED_USE_OPENTHREAD
  std::optional<InstanceLock> lock;
  if (!try_acquire_ot_lock(&lock, 100)) {
    if (!acquire_ot_lock_blocking(&lock)) {
      return false;
    }
  }
  otInstance *instance = lock->get_instance();

  otRouterInfo parent_info{};
  uint16_t parent_rloc16 = 0xffff;
  if (otThreadGetParentInfo(instance, &parent_info) == OT_ERROR_NONE) {
    parent_rloc16 = parent_info.mRloc16;
  }

  otNeighborInfoIterator it = OT_NEIGHBOR_INFO_ITERATOR_INIT;
  otNeighborInfo neighbor{};
  while (otThreadGetNextNeighborInfo(instance, &it, &neighbor) == OT_ERROR_NONE) {
    if (neighbor.mIsChild) {
      continue;
    }
    if (neighbor.mRloc16 == parent_rloc16) {
      continue;
    }

    RouterNeighborInfo info{};
    info.rloc16 = neighbor.mRloc16;
    info.average_rssi = neighbor.mAverageRssi;
    info.last_rssi = neighbor.mLastRssi;
    info.link_margin = neighbor.mLinkMargin;
    info.age_ms = neighbor.mAge * 1000U;
    cb(context, info);
  }

  return true;
#else
  (void) context;
  return false;
#endif
}

bool EspHomeOpenThreadPlatformAdapter::request_parent_search() {
#ifdef BIPARENTAL_ED_USE_OPENTHREAD
  std::optional<InstanceLock> lock;
  if (!try_acquire_ot_lock(&lock, 100)) {
    if (!acquire_ot_lock_blocking(&lock)) {
      return false;
    }
  }
  otInstance *instance = lock->get_instance();

  const otError err = otThreadSearchForBetterParent(instance);
  if (err != OT_ERROR_NONE) {
    ESP_LOGD(TAG, "otThreadSearchForBetterParent failed: %d", static_cast<int>(err));
    return false;
  }
  return true;
#else
  return false;
#endif
}

bool EspHomeOpenThreadPlatformAdapter::request_failover_to_preferred(uint16_t preferred_rloc16) {
  // Milestone 5 will implement preferred-parent biasing.
  (void) preferred_rloc16;
  ESP_LOGD(TAG, "Preferred failover requested but not implemented (Milestone 5)");
  return false;
}

bool EspHomeOpenThreadPlatformAdapter::request_failover_generic() {
#ifdef BIPARENTAL_ED_USE_OPENTHREAD
  std::optional<InstanceLock> lock;
  if (!try_acquire_ot_lock(&lock, 100)) {
    if (!acquire_ot_lock_blocking(&lock)) {
      ESP_LOGW(TAG, "Generic failover request failed: OT lock unavailable");
      return false;
    }
  }

  otInstance *instance = lock->get_instance();
  const otDeviceRole role = otThreadGetDeviceRole(instance);
  if (role == OT_DEVICE_ROLE_DISABLED) {
    ESP_LOGW(TAG, "Generic failover request failed: Thread is disabled");
    return false;
  }

  if (role != OT_DEVICE_ROLE_DETACHED) {
    const otError detach_err = otThreadBecomeDetached(instance);
    if (detach_err != OT_ERROR_NONE) {
      ESP_LOGW(TAG, "Generic failover detach failed: %d", static_cast<int>(detach_err));
      return false;
    }

    ESP_LOGI(TAG, "Generic failover request accepted (detaching)");
    return true;
  }

  // Already detached: explicitly trigger child-attach attempt.
  const otError child_err = otThreadBecomeChild(instance);
  if (child_err != OT_ERROR_NONE) {
    ESP_LOGW(TAG, "Generic failover reattach failed: %d", static_cast<int>(child_err));
    return false;
  }

  ESP_LOGI(TAG, "Generic failover request accepted (reattach as child)");
  return true;
#else
  ESP_LOGD(TAG, "Generic failover request ignored: OpenThread not available");
  return false;
#endif
}

}  // namespace biparental_ed
}  // namespace esphome
