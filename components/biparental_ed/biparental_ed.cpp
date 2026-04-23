#include "biparental_ed.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace biparental_ed {

static const char *const TAG = "biparental_ed";

void BiparentalEDComponent::setup() {
  this->apply_runtime_configuration_();

  if (this->enable_parent_response_callback_) {
    ESP_LOGD(TAG,
             "Parent response callback integration requested. "
             "OpenThread registration will be connected in a runtime adapter.");
  }

  ESP_LOGI(TAG, "Milestone 3 scaffold initialized");
}

void BiparentalEDComponent::loop() {
  // Callback-driven integration point for future OpenThread hook registrations.
  // Milestone 4+ will wire parent response callbacks into CandidateManager.
}

void BiparentalEDComponent::update() {
  const uint32_t now_ms = millis();

  ParentMetrics metrics{};
  const bool got_metrics = this->ot_adapter_->read_parent_metrics(&metrics);
  if (!got_metrics) {
    metrics.valid = false;
  }

  this->parent_health_monitor_.evaluate(now_ms, metrics);
  this->candidate_manager_.tick(now_ms);

  ParentCandidate standby{};
  const bool standby_available = this->candidate_manager_.get_standby(now_ms, &standby);

  const bool attached_as_child = this->ot_adapter_->is_attached_as_child();
  const auto action = this->failover_controller_.evaluate(now_ms, attached_as_child, standby_available,
                                                          this->parent_health_monitor_.is_failed(),
                                                          this->parent_health_monitor_.is_degraded());

  this->maybe_issue_failover_action_(now_ms, action, standby, standby_available);
  this->publish_diagnostics_(standby_available, standby);
}

void BiparentalEDComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Biparental ED Component:");
  ESP_LOGCONFIG(TAG, "  degraded_rssi_threshold: %d", this->degraded_rssi_threshold_);
  ESP_LOGCONFIG(TAG, "  hard_failure_timeout_ms: %u", static_cast<unsigned>(this->hard_failure_timeout_ms_));
  ESP_LOGCONFIG(TAG, "  degrade_persist_time_ms: %u", static_cast<unsigned>(this->degrade_persist_time_ms_));
  ESP_LOGCONFIG(TAG, "  hold_down_time_ms: %u", static_cast<unsigned>(this->hold_down_time_ms_));
  ESP_LOGCONFIG(TAG, "  standby_refresh_interval_ms: %u", static_cast<unsigned>(this->standby_refresh_interval_ms_));
  ESP_LOGCONFIG(TAG, "  failover_eligible_delay_ms: %u", static_cast<unsigned>(this->failover_eligible_delay_ms_));
  ESP_LOGCONFIG(TAG, "  post_failover_stabilize_time_ms: %u",
                static_cast<unsigned>(this->post_failover_stabilize_time_ms_));
  ESP_LOGCONFIG(TAG, "  preferred_reattach_timeout_ms: %u",
                static_cast<unsigned>(this->preferred_reattach_timeout_ms_));
  ESP_LOGCONFIG(TAG, "  enable_parent_response_callback: %s",
                this->enable_parent_response_callback_ ? "true" : "false");
  ESP_LOGCONFIG(TAG, "  control_plane_error_threshold: %u",
                static_cast<unsigned>(this->control_plane_error_threshold_));
  ESP_LOGCONFIG(TAG, "  standby_replace_hysteresis: %d", this->standby_replace_hysteresis_);
}

void BiparentalEDComponent::apply_runtime_configuration_() {
  this->parent_health_monitor_.set_degraded_rssi_threshold(static_cast<int8_t>(this->degraded_rssi_threshold_));
  this->parent_health_monitor_.set_hard_failure_timeout_ms(this->hard_failure_timeout_ms_);
  this->parent_health_monitor_.set_degrade_persist_time_ms(this->degrade_persist_time_ms_);
  this->parent_health_monitor_.set_control_plane_error_threshold(this->control_plane_error_threshold_);

  this->candidate_manager_.set_standby_refresh_interval_ms(this->standby_refresh_interval_ms_);
  this->candidate_manager_.set_replace_hysteresis(static_cast<int8_t>(this->standby_replace_hysteresis_));

  this->failover_controller_.set_hold_down_time_ms(this->hold_down_time_ms_);
  this->failover_controller_.set_failover_eligible_delay_ms(this->failover_eligible_delay_ms_);
  this->failover_controller_.set_post_failover_stabilize_time_ms(this->post_failover_stabilize_time_ms_);
  this->failover_controller_.set_preferred_reattach_timeout_ms(this->preferred_reattach_timeout_ms_);
}

void BiparentalEDComponent::maybe_issue_failover_action_(uint32_t now_ms, const FailoverAction &action,
                                                          const ParentCandidate &standby, bool standby_available) {
  (void) now_ms;

  if (action.type == FailoverActionType::NONE) {
    return;
  }

  if (action.type == FailoverActionType::TRIGGER_PREFERRED_REATTACH && standby_available) {
    ESP_LOGW(TAG, "Requesting preferred failover to standby parent rloc=0x%04x", standby.rloc16);
    if (!this->ot_adapter_->request_failover_to_preferred(standby.rloc16)) {
      ESP_LOGW(TAG, "Preferred failover request not yet implemented in OT adapter, falling back to generic flow");
      this->ot_adapter_->request_failover_generic();
    }
    return;
  }

  if (action.type == FailoverActionType::TRIGGER_GENERIC_REATTACH) {
    ESP_LOGW(TAG, "Requesting generic reattach/failover");
    this->ot_adapter_->request_failover_generic();
  }
}

void BiparentalEDComponent::publish_diagnostics_(bool standby_available, const ParentCandidate &standby) {
  DiagnosticsSnapshot snapshot{};
  snapshot.failover_state = this->failover_controller_.state();
  snapshot.health_state = this->parent_health_monitor_.state();
  snapshot.fail_reason = this->parent_health_monitor_.fail_reason();
  snapshot.standby_valid = standby_available;
  snapshot.active_parent_rloc16 = this->candidate_manager_.active_parent_rloc16();
  snapshot.standby_parent_rloc16 = standby_available ? standby.rloc16 : 0xffff;
  snapshot.failover_count = this->failover_controller_.failover_count();

  this->diagnostics_publisher_.publish(snapshot);
}

}  // namespace biparental_ed
}  // namespace esphome
