#pragma once

#include <cstdint>

#include "failover_controller.h"
#include "parent_health_monitor.h"

namespace esphome {
namespace biparental_ed {

struct DiagnosticsSnapshot {
  FailoverState failover_state{FailoverState::BOOTSTRAP};
  ParentHealthState health_state{ParentHealthState::HEALTHY};
  ParentFailReason fail_reason{ParentFailReason::NONE};
  bool standby_valid{false};
  uint16_t active_parent_rloc16{0xffff};
  uint16_t standby_parent_rloc16{0xffff};
  uint32_t failover_count{0};
  uint32_t preferred_attempt_count{0};
  uint32_t preferred_success_count{0};
  uint32_t preferred_miss_count{0};
  uint16_t preferred_target_rloc16{0xffff};
  uint16_t preferred_attached_parent_rloc16{0xffff};
  PreferredReattachOutcome preferred_outcome{PreferredReattachOutcome::NONE};
  FailoverState preferred_result_state{FailoverState::BOOTSTRAP};

  // Metrics (Milestone 4)
  int8_t active_parent_average_rssi{-127};
  uint8_t active_parent_link_margin{0};
  uint32_t active_parent_age_ms{0};

  int8_t standby_score{-127};
  int8_t standby_rssi{-127};
  uint8_t standby_link_margin{0};
  uint32_t standby_freshness_ms{0};
};

class DiagnosticsPublisher {
 public:
  void set_verbose(bool value) { this->verbose_ = value; }
  void publish(const DiagnosticsSnapshot &snapshot);

 private:
  bool changed_(const DiagnosticsSnapshot &snapshot) const;

  bool has_last_snapshot_{false};
  DiagnosticsSnapshot last_snapshot_{};
  bool verbose_{true};
  uint32_t last_publish_ms_{0};
  uint32_t periodic_publish_ms_{5000};
};

}  // namespace biparental_ed
}  // namespace esphome
