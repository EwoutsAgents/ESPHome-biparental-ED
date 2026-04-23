#include "biparental_ed.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#ifdef USE_OPENTHREAD
#include "esphome/components/openthread/openthread.h"
#endif

namespace esphome {
namespace biparental_ed {

static const char *const TAG = "biparental_ed";

void BiparentalEDComponent::setup() {
  this->apply_runtime_configuration_();

#ifdef USE_OPENTHREAD
  // If OpenThread is configured in the user's YAML, ESPHome will set USE_OPENTHREAD and expose
  // the global OpenThread component instance. In that case we can enable the real adapter.
  if (esphome::openthread::global_openthread_component != nullptr) {
    this->ot_adapter_ = &this->esphome_ot_adapter_;
    ESP_LOGI(TAG, "OpenThread detected; enabling runtime adapter");
  }
#endif

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

  // Track active parent identity for candidate filtering.
  if (metrics.valid && metrics.parent_rloc16 != 0xffff &&
      metrics.parent_rloc16 != this->candidate_manager_.active_parent_rloc16()) {
    this->candidate_manager_.mark_failover_complete(now_ms, metrics.parent_rloc16);
  }

  this->parent_health_monitor_.evaluate(now_ms, metrics);
  this->candidate_manager_.tick(now_ms);

  this->maybe_scan_neighbors_(now_ms);
  this->maybe_trigger_parent_search_refresh_(now_ms);

  ParentCandidate standby{};
  const bool standby_available = this->candidate_manager_.get_standby(now_ms, &standby);

  const bool attached_as_child = this->ot_adapter_->is_attached_as_child();
  const auto action = this->failover_controller_.evaluate(now_ms, attached_as_child, standby_available,
                                                          this->parent_health_monitor_.is_failed(),
                                                          this->parent_health_monitor_.is_degraded());

  this->maybe_issue_failover_action_(now_ms, action, standby, standby_available);
  this->publish_diagnostics_(metrics, standby_available, standby);
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

  ESP_LOGCONFIG(TAG, "  neighbor_scan_interval_ms: %u", static_cast<unsigned>(this->neighbor_scan_interval_ms_));
  ESP_LOGCONFIG(TAG, "  neighbor_max_age_ms: %u", static_cast<unsigned>(this->neighbor_max_age_ms_));
  ESP_LOGCONFIG(TAG, "  parent_search_refresh_interval_ms: %u",
                static_cast<unsigned>(this->parent_search_refresh_interval_ms_));
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

void BiparentalEDComponent::maybe_scan_neighbors_(uint32_t now_ms) {
  if (this->neighbor_scan_interval_ms_ == 0) {
    return;
  }
  if (this->last_neighbor_scan_ms_ != 0 && (now_ms - this->last_neighbor_scan_ms_) < this->neighbor_scan_interval_ms_) {
    return;
  }

  this->last_neighbor_scan_ms_ = now_ms;

  struct Context {
    BiparentalEDComponent *self;
    uint32_t now_ms;
  } ctx{this, now_ms};

  OpenThreadPlatformAdapter::RouterNeighborCallback cb = [](void *context,
                                                            const OpenThreadPlatformAdapter::RouterNeighborInfo &info) {
    auto *ctx = static_cast<Context *>(context);
    if (info.age_ms > ctx->self->neighbor_max_age_ms_) {
      return;
    }
    ctx->self->candidate_manager_.observe_router_neighbor(ctx->now_ms, info.rloc16, static_cast<int8_t>(info.link_margin),
                                                          info.average_rssi);
  };

  (void) this->ot_adapter_->read_router_neighbors(cb, &ctx);
}

void BiparentalEDComponent::maybe_trigger_parent_search_refresh_(uint32_t now_ms) {
  if (this->parent_search_refresh_interval_ms_ == 0) {
    return;
  }
  if (this->last_parent_search_ms_ != 0 &&
      (now_ms - this->last_parent_search_ms_) < this->parent_search_refresh_interval_ms_) {
    return;
  }

  // WARNING: otThreadSearchForBetterParent() may cause the stack to automatically switch parents.
  // We expose this as an explicit, opt-in refresh mechanism.
  if (this->ot_adapter_->request_parent_search()) {
    this->last_parent_search_ms_ = now_ms;
    ESP_LOGD(TAG, "Triggered OpenThread better-parent search refresh");
  }
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

void BiparentalEDComponent::publish_diagnostics_(const ParentMetrics &metrics, bool standby_available,
                                                 const ParentCandidate &standby) {
  DiagnosticsSnapshot snapshot{};
  snapshot.failover_state = this->failover_controller_.state();
  snapshot.health_state = this->parent_health_monitor_.state();
  snapshot.fail_reason = this->parent_health_monitor_.fail_reason();
  snapshot.standby_valid = standby_available;
  snapshot.active_parent_rloc16 = metrics.valid ? metrics.parent_rloc16 : this->candidate_manager_.active_parent_rloc16();
  snapshot.standby_parent_rloc16 = standby_available ? standby.rloc16 : 0xffff;
  snapshot.failover_count = this->failover_controller_.failover_count();

  snapshot.active_parent_average_rssi = metrics.valid ? metrics.average_rssi : -127;
  snapshot.active_parent_link_margin = metrics.valid ? metrics.parent_link_margin : 0;
  snapshot.active_parent_age_ms = metrics.valid ? metrics.parent_age_ms : 0;

  snapshot.standby_score = standby_available ? standby.score : -127;
  snapshot.standby_rssi = standby_available ? standby.rssi : -127;
  snapshot.standby_link_margin = standby_available ? static_cast<uint8_t>(standby.link_margin) : 0;
  snapshot.standby_freshness_ms = standby_available ? this->candidate_manager_.standby_freshness_ms(millis()) : 0;

  this->diagnostics_publisher_.publish(snapshot);
}

}  // namespace biparental_ed
}  // namespace esphome
