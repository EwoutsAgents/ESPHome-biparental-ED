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
};

}  // namespace biparental_ed
}  // namespace esphome
