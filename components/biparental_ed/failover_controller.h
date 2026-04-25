#pragma once

#include <cstdint>

namespace esphome {
namespace biparental_ed {

enum class FailoverState : uint8_t {
  BOOTSTRAP = 0,
  ATTACHING_INITIAL = 1,
  ATTACHED_ACTIVE_NO_STANDBY = 2,
  ATTACHED_ACTIVE_STANDBY_WARM = 3,
  FAILOVER_ELIGIBLE = 4,
  FAILOVER_TRIGGERED = 5,
  REATTACHING_PREFERRED = 6,
  REATTACHING_GENERIC = 7,
  POST_FAILOVER_STABILIZE = 8,
};

enum class FailoverActionType : uint8_t {
  NONE = 0,
  TRIGGER_PREFERRED_REATTACH = 1,
  TRIGGER_GENERIC_REATTACH = 2,
};

struct FailoverAction {
  FailoverActionType type{FailoverActionType::NONE};
};

class FailoverController {
 public:
  void set_hold_down_time_ms(uint32_t value) { this->hold_down_time_ms_ = value; }
  void set_failover_eligible_delay_ms(uint32_t value) { this->failover_eligible_delay_ms_ = value; }
  void set_post_failover_stabilize_time_ms(uint32_t value) { this->post_failover_stabilize_time_ms_ = value; }
  void set_preferred_reattach_timeout_ms(uint32_t value) { this->preferred_reattach_timeout_ms_ = value; }

  FailoverAction evaluate(uint32_t now_ms, bool attached_as_child, bool standby_available, bool health_failed,
                          bool health_degraded);

  FailoverState state() const { return this->state_; }
  uint32_t failover_count() const { return this->failover_count_; }
  uint32_t preferred_attempt_count() const { return this->preferred_attempt_count_; }
  uint32_t preferred_success_count() const { return this->preferred_success_count_; }
  uint32_t preferred_miss_count() const { return this->preferred_miss_count_; }

 private:
  void transition_(FailoverState next_state, uint32_t now_ms);
  bool hold_down_expired_(uint32_t now_ms) const;

  FailoverState state_{FailoverState::BOOTSTRAP};
  uint32_t state_entered_ms_{0};
  uint32_t failover_eligible_since_ms_{0};
  uint32_t last_failover_ms_{0};
  uint32_t failover_count_{0};
  uint32_t preferred_attempt_count_{0};
  uint32_t preferred_success_count_{0};
  uint32_t preferred_miss_count_{0};

  uint32_t hold_down_time_ms_{30000};
  uint32_t failover_eligible_delay_ms_{10000};
  uint32_t post_failover_stabilize_time_ms_{10000};
  uint32_t preferred_reattach_timeout_ms_{5000};
};

}  // namespace biparental_ed
}  // namespace esphome
