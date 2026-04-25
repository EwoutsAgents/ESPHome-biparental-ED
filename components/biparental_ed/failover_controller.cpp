#include "failover_controller.h"

namespace esphome {
namespace biparental_ed {

FailoverAction FailoverController::evaluate(uint32_t now_ms, bool attached_as_child, bool standby_available,
                                            bool health_failed, bool health_degraded, uint16_t attached_parent_rloc16,
                                            uint16_t standby_parent_rloc16) {
  FailoverAction action;
  const bool attached_parent_known = attached_parent_rloc16 != 0xffff && attached_parent_rloc16 != 0xfffe;

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
        this->preferred_attempt_count_++;
        this->preferred_current_target_rloc16_ = standby_parent_rloc16;
        action.type = FailoverActionType::TRIGGER_PREFERRED_REATTACH;
        action.preferred_target_rloc16 = this->preferred_current_target_rloc16_;
      } else {
        this->transition_(FailoverState::REATTACHING_GENERIC, now_ms);
        action.type = FailoverActionType::TRIGGER_GENERIC_REATTACH;
        action.reason = FailoverActionReason::NO_STANDBY_AVAILABLE;
      }
      break;

    case FailoverState::REATTACHING_PREFERRED:
      if (attached_as_child) {
        if (attached_parent_known && attached_parent_rloc16 == this->preferred_current_target_rloc16_) {
          this->transition_(FailoverState::POST_FAILOVER_STABILIZE, now_ms);
          this->last_failover_ms_ = now_ms;
          this->failover_count_++;
          this->preferred_success_count_++;
          this->record_preferred_outcome_(PreferredReattachOutcome::SUCCESS, this->preferred_current_target_rloc16_,
                                          attached_parent_rloc16, FailoverState::POST_FAILOVER_STABILIZE);
          this->preferred_current_target_rloc16_ = 0xffff;
          break;
        }

        // Child attach happened but to a different parent: deterministic miss -> generic fallback.
        if (attached_parent_known) {
          this->transition_(FailoverState::REATTACHING_GENERIC, now_ms);
          this->preferred_miss_count_++;
          this->record_preferred_outcome_(PreferredReattachOutcome::MISS, this->preferred_current_target_rloc16_,
                                          attached_parent_rloc16, FailoverState::REATTACHING_GENERIC);
          action.type = FailoverActionType::TRIGGER_GENERIC_REATTACH;
          action.reason = FailoverActionReason::PREFERRED_MISS;
          action.preferred_target_rloc16 = this->preferred_last_target_rloc16_;
          action.attached_parent_rloc16 = attached_parent_rloc16;
          this->preferred_current_target_rloc16_ = 0xffff;
          break;
        }
      }

      if ((now_ms - this->state_entered_ms_) >= this->preferred_reattach_timeout_ms_) {
        this->transition_(FailoverState::REATTACHING_GENERIC, now_ms);
        this->preferred_miss_count_++;
        action.type = FailoverActionType::TRIGGER_GENERIC_REATTACH;
        action.preferred_target_rloc16 = this->preferred_current_target_rloc16_;

        if (!attached_as_child) {
          this->record_preferred_outcome_(PreferredReattachOutcome::TIMEOUT, this->preferred_current_target_rloc16_,
                                          0xffff, FailoverState::REATTACHING_GENERIC);
          action.reason = FailoverActionReason::PREFERRED_TIMEOUT;
          action.attached_parent_rloc16 = 0xffff;
        } else {
          // Child attach happened, but parent identity could not be resolved before timeout;
          // treat as miss and force deterministic generic fallback.
          this->record_preferred_outcome_(PreferredReattachOutcome::MISS, this->preferred_current_target_rloc16_, 0xffff,
                                          FailoverState::REATTACHING_GENERIC);
          action.reason = FailoverActionReason::PREFERRED_MISS;
          action.attached_parent_rloc16 = 0xffff;
        }

        this->preferred_current_target_rloc16_ = 0xffff;
        break;
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

void FailoverController::record_preferred_outcome_(PreferredReattachOutcome outcome, uint16_t target_rloc16,
                                                   uint16_t attached_parent_rloc16, FailoverState result_state) {
  this->preferred_last_outcome_ = outcome;
  this->preferred_last_target_rloc16_ = target_rloc16;
  this->preferred_last_attached_parent_rloc16_ = attached_parent_rloc16;
  this->preferred_last_result_state_ = result_state;
  this->preferred_outcome_event_count_++;
}

bool FailoverController::hold_down_expired_(uint32_t now_ms) const {
  return (now_ms - this->last_failover_ms_) >= this->hold_down_time_ms_;
}

}  // namespace biparental_ed
}  // namespace esphome
