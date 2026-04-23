#pragma once

#include <cstdint>

namespace esphome {
namespace biparental_ed {

enum class ParentHealthState : uint8_t {
  HEALTHY = 0,
  DEGRADED = 1,
  FAILED = 2,
};

enum class ParentFailReason : uint8_t {
  NONE = 0,
  HARD_TIMEOUT = 1,
  LINK_DEGRADED = 2,
  CONTROL_PLANE = 3,
};

struct ParentMetrics {
  bool valid{false};
  int8_t average_rssi{-127};
  int8_t last_rssi{-127};
  bool supervision_ok{true};
  uint32_t control_plane_error_count{0};
  uint32_t last_parent_rx_ms{0};
};

class ParentHealthMonitor {
 public:
  void set_degraded_rssi_threshold(int8_t value) { this->degraded_rssi_threshold_ = value; }
  void set_hard_failure_timeout_ms(uint32_t value) { this->hard_failure_timeout_ms_ = value; }
  void set_degrade_persist_time_ms(uint32_t value) { this->degrade_persist_time_ms_ = value; }
  void set_control_plane_error_threshold(uint32_t value) { this->control_plane_error_threshold_ = value; }

  void evaluate(uint32_t now_ms, const ParentMetrics &metrics);

  ParentHealthState state() const { return this->state_; }
  ParentFailReason fail_reason() const { return this->fail_reason_; }
  bool is_degraded() const { return this->state_ == ParentHealthState::DEGRADED; }
  bool is_failed() const { return this->state_ == ParentHealthState::FAILED; }

 private:
  int8_t degraded_rssi_threshold_{-70};
  uint32_t hard_failure_timeout_ms_{15000};
  uint32_t degrade_persist_time_ms_{10000};
  uint32_t control_plane_error_threshold_{3};

  uint32_t degraded_since_ms_{0};
  ParentHealthState state_{ParentHealthState::HEALTHY};
  ParentFailReason fail_reason_{ParentFailReason::NONE};
};

}  // namespace biparental_ed
}  // namespace esphome
