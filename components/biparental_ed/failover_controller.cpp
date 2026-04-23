#include "failover_controller.h"

namespace esphome {
namespace biparental_ed {

FailoverAction FailoverController::evaluate(uint32_t now_ms, bool attached_as_child, bool standby_available,
                                            bool health_failed, bool health_degraded) {
  FailoverAction action;

  switch (this->state_) {
    case FailoverState::BOOTSTRAP:
      this->transition_(FailoverState::ATTACHING_INITIAL, now_ms);
      break;

    case FailoverState::ATTACHING_INITIAL:
      if (attached_as_child) {
        this->transition_(standby_available ? FailoverState::ATTACHED_ACTIVE_STANDBY_WARM
                                            : FailoverState::ATTACHED_ACTIVE_NO_STANDBY,
                          now_ms);
      }
      break;

    case FailoverState::ATTACHED_ACTIVE_NO_STANDBY:
      if (!attached_as_child) {
        this->transition_(FailoverState::ATTACHING_INITIAL, now_ms);
        break;
      }
      if (standby_available) {
        this->transition_(FailoverState::ATTACHED_ACTIVE_STANDBY_WARM, now_ms);
        break;
      }
      if (health_failed && this->hold_down_expired_(now_ms)) {
        this->transition_(FailoverState::FAILOVER_TRIGGERED, now_ms);
      }
      break;

    case FailoverState::ATTACHED_ACTIVE_STANDBY_WARM:
      if (!attached_as_child) {
        this->transition_(FailoverState::ATTACHING_INITIAL, now_ms);
        break;
      }
      if (!standby_available) {
        this->transition_(FailoverState::ATTACHED_ACTIVE_NO_STANDBY, now_ms);
        break;
      }
      if (health_failed && this->hold_down_expired_(now_ms)) {
        this->transition_(FailoverState::FAILOVER_TRIGGERED, now_ms);
        break;
      }
      if (health_degraded && this->hold_down_expired_(now_ms)) {
        this->transition_(FailoverState::FAILOVER_ELIGIBLE, now_ms);
        this->failover_eligible_since_ms_ = now_ms;
      }
      break;

    case FailoverState::FAILOVER_ELIGIBLE:
      if (!attached_as_child) {
        this->transition_(FailoverState::ATTACHING_INITIAL, now_ms);
        break;
      }
      if (!health_degraded) {
        this->transition_(standby_available ? FailoverState::ATTACHED_ACTIVE_STANDBY_WARM
                                            : FailoverState::ATTACHED_ACTIVE_NO_STANDBY,
                          now_ms);
        break;
      }
      if ((now_ms - this->failover_eligible_since_ms_) >= this->failover_eligible_delay_ms_) {
        this->transition_(FailoverState::FAILOVER_TRIGGERED, now_ms);
      }
      break;

    case FailoverState::FAILOVER_TRIGGERED:
      if (standby_available) {
        this->transition_(FailoverState::REATTACHING_PREFERRED, now_ms);
        action.type = FailoverActionType::TRIGGER_PREFERRED_REATTACH;
      } else {
        this->transition_(FailoverState::REATTACHING_GENERIC, now_ms);
        action.type = FailoverActionType::TRIGGER_GENERIC_REATTACH;
      }
      break;

    case FailoverState::REATTACHING_PREFERRED:
      if (attached_as_child) {
        this->transition_(FailoverState::POST_FAILOVER_STABILIZE, now_ms);
        this->last_failover_ms_ = now_ms;
        this->failover_count_++;
        break;
      }
      if ((now_ms - this->state_entered_ms_) >= this->preferred_reattach_timeout_ms_) {
        this->transition_(FailoverState::REATTACHING_GENERIC, now_ms);
        action.type = FailoverActionType::TRIGGER_GENERIC_REATTACH;
      }
      break;

    case FailoverState::REATTACHING_GENERIC:
      if (attached_as_child) {
        this->transition_(FailoverState::POST_FAILOVER_STABILIZE, now_ms);
        this->last_failover_ms_ = now_ms;
        this->failover_count_++;
      }
      break;

    case FailoverState::POST_FAILOVER_STABILIZE:
      if (!attached_as_child) {
        this->transition_(FailoverState::ATTACHING_INITIAL, now_ms);
        break;
      }
      if ((now_ms - this->state_entered_ms_) >= this->post_failover_stabilize_time_ms_) {
        this->transition_(standby_available ? FailoverState::ATTACHED_ACTIVE_STANDBY_WARM
                                            : FailoverState::ATTACHED_ACTIVE_NO_STANDBY,
                          now_ms);
      }
      break;
  }

  return action;
}

void FailoverController::transition_(FailoverState next_state, uint32_t now_ms) {
  this->state_ = next_state;
  this->state_entered_ms_ = now_ms;
}

bool FailoverController::hold_down_expired_(uint32_t now_ms) const {
  return (now_ms - this->last_failover_ms_) >= this->hold_down_time_ms_;
}

}  // namespace biparental_ed
}  // namespace esphome
