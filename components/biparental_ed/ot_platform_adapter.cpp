#include "ot_platform_adapter.h"

#include <optional>
#include <utility>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#ifdef USE_OPENTHREAD
#include "esphome/components/openthread/openthread.h"
#include <openthread/thread.h>
#endif

namespace esphome {
namespace biparental_ed {

static const char *const TAG = "biparental_ed.ot";

#ifdef USE_OPENTHREAD
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
#endif

bool EspHomeOpenThreadPlatformAdapter::is_attached_as_child() const {
#ifdef USE_OPENTHREAD
  std::optional<InstanceLock> lock;
  if (!try_acquire_ot_lock(&lock, 0)) {
    return false;
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
#ifdef USE_OPENTHREAD
  std::optional<InstanceLock> lock;
  if (!try_acquire_ot_lock(&lock, 10)) {
    return false;
  }

  otInstance *instance = lock->get_instance();

  otRouterInfo parent_info{};
  const otError parent_info_err = otThreadGetParentInfo(instance, &parent_info);
  if (parent_info_err != OT_ERROR_NONE) {
    return false;
  }

  int8_t avg_rssi = -127;
  int8_t last_rssi = -127;
  (void) otThreadGetParentAverageRssi(instance, &avg_rssi);
  (void) otThreadGetParentLastRssi(instance, &last_rssi);

  // Find parent in neighbor table for age/link margin.
  otNeighborInfoIterator it = OT_NEIGHBOR_INFO_ITERATOR_INIT;
  otNeighborInfo neighbor{};
  uint32_t parent_age_ms = 0;
  uint8_t parent_link_margin = 0;
  while (otThreadGetNextNeighborInfo(instance, &it, &neighbor) == OT_ERROR_NONE) {
    if (neighbor.mRloc16 == parent_info.mRloc16) {
      parent_age_ms = neighbor.mAge * 1000U;
      parent_link_margin = neighbor.mLinkMargin;
      break;
    }
  }

  metrics->valid = true;
  metrics->average_rssi = avg_rssi;
  metrics->last_rssi = last_rssi;
  metrics->parent_rloc16 = parent_info.mRloc16;
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
#ifdef USE_OPENTHREAD
  std::optional<InstanceLock> lock;
  if (!try_acquire_ot_lock(&lock, 10)) {
    return false;
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
#ifdef USE_OPENTHREAD
  std::optional<InstanceLock> lock;
  if (!try_acquire_ot_lock(&lock, 10)) {
    return false;
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
  // Milestone 5 will implement deterministic failover.
  ESP_LOGD(TAG, "Generic failover requested but not implemented (Milestone 5)");
  return false;
}

}  // namespace biparental_ed
}  // namespace esphome
