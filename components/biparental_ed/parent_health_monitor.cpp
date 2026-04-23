#include "parent_health_monitor.h"

namespace esphome {
namespace biparental_ed {

void ParentHealthMonitor::evaluate(uint32_t now_ms, const ParentMetrics &metrics) {
  if (!metrics.valid) {
    this->state_ = ParentHealthState::FAILED;
    this->fail_reason_ = ParentFailReason::HARD_TIMEOUT;
    return;
  }

  const bool hard_timeout = (now_ms - metrics.last_parent_rx_ms) >= this->hard_failure_timeout_ms_;
  const bool supervision_failed = !metrics.supervision_ok;
  if (hard_timeout || supervision_failed) {
    this->state_ = ParentHealthState::FAILED;
    this->fail_reason_ = ParentFailReason::HARD_TIMEOUT;
    return;
  }

  const bool link_degraded = metrics.average_rssi < this->degraded_rssi_threshold_;
  const bool control_plane_unstable = metrics.control_plane_error_count >= this->control_plane_error_threshold_;

  if (link_degraded || control_plane_unstable) {
    if (this->degraded_since_ms_ == 0) {
      this->degraded_since_ms_ = now_ms;
    }
    if ((now_ms - this->degraded_since_ms_) >= this->degrade_persist_time_ms_) {
      this->state_ = ParentHealthState::DEGRADED;
      this->fail_reason_ = control_plane_unstable ? ParentFailReason::CONTROL_PLANE : ParentFailReason::LINK_DEGRADED;
      return;
    }
  } else {
    this->degraded_since_ms_ = 0;
  }

  this->state_ = ParentHealthState::HEALTHY;
  this->fail_reason_ = ParentFailReason::NONE;
}

}  // namespace biparental_ed
}  // namespace esphome
