#include "biparental_ed.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <cstdio>

#ifdef USE_OPENTHREAD
#include "esphome/components/openthread/openthread.h"
#include <openthread/thread.h>
#endif

namespace esphome {
namespace biparental_ed {

static const char *const TAG = "biparental_ed";

#ifdef USE_OPENTHREAD
static void parent_response_callback(otThreadParentResponseInfo *info, void *context) {
  if (info == nullptr || context == nullptr) {
    return;
  }
  auto *self = static_cast<BiparentalEDComponent *>(context);
  self->observe_parent_response_(info->mRloc16, info->mRssi, info->mLinkQuality3, info->mLinkQuality2,
                                 info->mLinkQuality1, info->mIsAttached);
}
#endif

static const char *preferred_outcome_to_cstr(PreferredReattachOutcome outcome) {
  switch (outcome) {
    case PreferredReattachOutcome::SUCCESS:
      return "success";
    case PreferredReattachOutcome::MISS:
      return "miss";
    case PreferredReattachOutcome::TIMEOUT:
      return "timeout";
    case PreferredReattachOutcome::NONE:
    default:
      return "none";
  }
}

static const char *action_reason_to_cstr(FailoverActionReason reason) {
  switch (reason) {
    case FailoverActionReason::NO_STANDBY_AVAILABLE:
      return "no_standby";
    case FailoverActionReason::PREFERRED_MISS:
      return "preferred_miss";
    case FailoverActionReason::PREFERRED_TIMEOUT:
      return "preferred_timeout";
    case FailoverActionReason::NONE:
    default:
      return "none";
  }
}

static const char *fail_reason_to_cstr(ParentFailReason reason) {
  switch (reason) {
    case ParentFailReason::HARD_TIMEOUT:
      return "hard_timeout";
    case ParentFailReason::LINK_DEGRADED:
      return "link_degraded";
    case ParentFailReason::CONTROL_PLANE:
      return "control_plane";
    case ParentFailReason::NONE:
    default:
      return "none";
  }
}

static void format_rloc16(char *buffer, size_t buffer_size, uint16_t rloc16) {
  if (rloc16 == 0xffff || rloc16 == 0xfffe) {
    snprintf(buffer, buffer_size, "unknown");
    return;
  }
  snprintf(buffer, buffer_size, "0x%04X", rloc16);
}

void BiparentalEDComponent::setup() {
  this->apply_runtime_configuration_();
  this->diagnostics_publisher_.set_verbose(this->verbose_diagnostics_);
  this->diagnostics_publisher_.set_periodic_publish_ms(this->diagnostics_publish_interval_ms_);

#ifdef USE_OPENTHREAD
  // If OpenThread is configured in the user's YAML, ESPHome will set USE_OPENTHREAD and expose
  // the global OpenThread component instance. In that case we can enable the real adapter.
  if (esphome::openthread::global_openthread_component != nullptr) {
    this->ot_adapter_ = &this->esphome_ot_adapter_;
    ESP_LOGI(TAG, "OpenThread detected; enabling runtime adapter");
  }
#endif

  if (this->enable_parent_response_callback_) {
#ifdef USE_OPENTHREAD
    if (esphome::openthread::global_openthread_component != nullptr &&
        esphome::openthread::global_openthread_component->is_lock_initialized()) {
      auto lock = esphome::openthread::InstanceLock::acquire();
      otThreadRegisterParentResponseCallback(lock.get_instance(), parent_response_callback, this);
      this->parent_response_callback_registered_ = true;
      ESP_LOGI(TAG, "Registered OpenThread parent response callback");
    } else {
      ESP_LOGD(TAG, "Parent response callback requested; waiting for OpenThread lock");
    }
#else
    ESP_LOGD(TAG, "Parent response callback requested but OpenThread is not enabled");
#endif
  }

  ESP_LOGI(TAG, "Milestone 3 scaffold initialized");
}

void BiparentalEDComponent::loop() {
#ifdef USE_OPENTHREAD
  if (this->enable_parent_response_callback_ && !this->parent_response_callback_registered_ &&
      esphome::openthread::global_openthread_component != nullptr &&
      esphome::openthread::global_openthread_component->is_lock_initialized()) {
    auto lock = esphome::openthread::InstanceLock::acquire();
    otThreadRegisterParentResponseCallback(lock.get_instance(), parent_response_callback, this);
    this->parent_response_callback_registered_ = true;
    ESP_LOGI(TAG, "Registered OpenThread parent response callback");
  }
#endif
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
  const uint16_t attached_parent_rloc16_for_policy =
      (metrics.valid && metrics.parent_rloc16 != 0xffff && metrics.parent_rloc16 != 0xfffe) ? metrics.parent_rloc16 : 0xffff;
  const uint16_t standby_parent_rloc16_for_policy = standby_available ? standby.rloc16 : 0xffff;

  static uint32_t last_update_debug_ms = 0;
  if (this->runtime_debug_enabled_ &&
      (last_update_debug_ms == 0 || (now_ms - last_update_debug_ms) >= this->runtime_debug_interval_ms_)) {
#ifdef USE_OPENTHREAD
    const bool ot_present = esphome::openthread::global_openthread_component != nullptr;
    const bool ot_lock_initialized = ot_present && esphome::openthread::global_openthread_component->is_lock_initialized();
    const bool ot_connected = ot_present && esphome::openthread::global_openthread_component->is_connected();

    bool ot_lock_acquired = false;
    int ot_role = -1;
    int ot_parent_info_err = -1;
    uint16_t ot_parent_rloc = 0xffff;
    if (ot_present && ot_lock_initialized) {
      auto lock = esphome::openthread::InstanceLock::try_acquire(100);
      if (lock) {
        ot_lock_acquired = true;
        otInstance *instance = lock->get_instance();
        ot_role = static_cast<int>(otThreadGetDeviceRole(instance));
        otRouterInfo parent_info{};
        ot_parent_info_err = static_cast<int>(otThreadGetParentInfo(instance, &parent_info));
        if (ot_parent_info_err == static_cast<int>(OT_ERROR_NONE)) {
          ot_parent_rloc = parent_info.mRloc16;
        }
      }
    }
#else
    const bool ot_present = false;
    const bool ot_lock_initialized = false;
    const bool ot_connected = false;
    const bool ot_lock_acquired = false;
    const int ot_role = -1;
    const int ot_parent_info_err = -1;
    const uint16_t ot_parent_rloc = 0xffff;
#endif

    ESP_LOGD(TAG,
             "runtime: got_metrics=%s metrics.valid=%s parent=0x%04x rssi=%d lm=%u age=%ums attached_as_child=%s standby=%s ot_present=%s ot_lock_init=%s ot_connected=%s ot_lock=%s ot_role=%d ot_parent_err=%d ot_parent=0x%04x",
             got_metrics ? "true" : "false", metrics.valid ? "true" : "false", metrics.parent_rloc16,
             metrics.average_rssi, static_cast<unsigned>(metrics.parent_link_margin),
             static_cast<unsigned>(metrics.parent_age_ms), attached_as_child ? "true" : "false",
             standby_available ? "true" : "false", ot_present ? "true" : "false",
             ot_lock_initialized ? "true" : "false", ot_connected ? "true" : "false",
             ot_lock_acquired ? "true" : "false", ot_role, ot_parent_info_err, ot_parent_rloc);
    last_update_debug_ms = now_ms;
  }

  const auto action = this->failover_controller_.evaluate(now_ms, attached_as_child, standby_available,
                                                          this->parent_health_monitor_.is_failed(),
                                                          this->parent_health_monitor_.is_degraded(),
                                                          attached_parent_rloc16_for_policy,
                                                          standby_parent_rloc16_for_policy);

  this->maybe_log_preferred_outcome_();
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
  ESP_LOGCONFIG(TAG, "  runtime_debug_enabled: %s", this->runtime_debug_enabled_ ? "true" : "false");
  ESP_LOGCONFIG(TAG, "  runtime_debug_interval_ms: %u", static_cast<unsigned>(this->runtime_debug_interval_ms_));
  ESP_LOGCONFIG(TAG, "  verbose_diagnostics: %s", this->verbose_diagnostics_ ? "true" : "false");
  ESP_LOGCONFIG(TAG, "  diagnostics_publish_interval_ms: %u",
                static_cast<unsigned>(this->diagnostics_publish_interval_ms_));
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
    uint8_t seen{0};
    uint8_t fresh{0};
    uint8_t stale{0};
  } ctx{this, now_ms};

  OpenThreadPlatformAdapter::RouterNeighborCallback cb = [](void *context,
                                                            const OpenThreadPlatformAdapter::RouterNeighborInfo &info) {
    auto *ctx = static_cast<Context *>(context);
    ctx->seen++;
    if (info.age_ms > ctx->self->neighbor_max_age_ms_) {
      ctx->stale++;
      return;
    }
    ctx->fresh++;
    ctx->self->candidate_manager_.observe_router_neighbor(ctx->now_ms, info.rloc16, static_cast<int8_t>(info.link_margin),
                                                          info.average_rssi);
  };

  (void) this->ot_adapter_->read_router_neighbors(cb, &ctx);
}

void BiparentalEDComponent::observe_parent_response_(uint16_t rloc16, int8_t rssi, uint8_t link_quality_3,
                                                     uint8_t link_quality_2, uint8_t link_quality_1,
                                                     bool is_attached) {
  if (rloc16 == 0xffff || rloc16 == 0xfffe) {
    return;
  }

  // Parent Response only exposes RSSI and link-quality counters, not an OT link-margin value.
  // Convert the counters to a conservative 0..30-ish margin proxy so CandidateManager can rank
  // callback-derived candidates without depending on the MTD neighbor table.
  const uint8_t link_margin = static_cast<uint8_t>((link_quality_3 * 10U) + (link_quality_2 * 6U) +
                                                  (link_quality_1 * 3U));
  this->candidate_manager_.observe_parent_response(millis(), rloc16, static_cast<int8_t>(link_margin), rssi);
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

  if (action.reason == FailoverActionReason::PREFERRED_MISS || action.reason == FailoverActionReason::PREFERRED_TIMEOUT ||
      action.reason == FailoverActionReason::NO_STANDBY_AVAILABLE) {
    this->last_failover_reason_ = action_reason_to_cstr(action.reason);
  } else {
    this->last_failover_reason_ = fail_reason_to_cstr(this->parent_health_monitor_.fail_reason());
  }

  if (action.type == FailoverActionType::TRIGGER_PREFERRED_REATTACH && standby_available) {
    const uint16_t target_rloc16 = action.preferred_target_rloc16 != 0xffff ? action.preferred_target_rloc16 : standby.rloc16;
    ESP_LOGW(TAG, "Requesting best-effort preferred parent search (target=0x%04x)", target_rloc16);
    if (!this->ot_adapter_->request_failover_to_preferred(target_rloc16)) {
      ESP_LOGW(TAG, "Best-effort preferred parent search could not be started, falling back to generic reattach");
      this->ot_adapter_->request_failover_generic();
    }
    return;
  }

  if (action.type == FailoverActionType::TRIGGER_GENERIC_REATTACH) {
    if (action.reason == FailoverActionReason::PREFERRED_MISS || action.reason == FailoverActionReason::PREFERRED_TIMEOUT) {
      ESP_LOGW(TAG,
               "Preferred outcome=%s target=0x%04x attached=0x%04x -> action=generic_reattach",
               action.reason == FailoverActionReason::PREFERRED_MISS ? "miss" : "timeout",
               action.preferred_target_rloc16,
               action.attached_parent_rloc16);
    } else {
      ESP_LOGW(TAG, "Requesting generic reattach/failover (reason=%s)", action_reason_to_cstr(action.reason));
    }
    this->ot_adapter_->request_failover_generic();
  }
}

void BiparentalEDComponent::maybe_log_preferred_outcome_() {
  const uint32_t event_count = this->failover_controller_.preferred_outcome_event_count();
  if (event_count == 0 || event_count == this->last_logged_preferred_outcome_event_count_) {
    return;
  }

  this->last_logged_preferred_outcome_event_count_ = event_count;
  ESP_LOGI(TAG,
           "Preferred outcome=%s target=0x%04x attached=0x%04x -> result_state=%u",
           preferred_outcome_to_cstr(this->failover_controller_.preferred_last_outcome()),
           this->failover_controller_.preferred_last_target_rloc16(),
           this->failover_controller_.preferred_last_attached_parent_rloc16(),
           static_cast<unsigned>(this->failover_controller_.preferred_last_result_state()));
}

void BiparentalEDComponent::publish_diagnostics_(const ParentMetrics &metrics, bool standby_available,
                                                 const ParentCandidate &standby) {
  char active_parent_id[16];
  format_rloc16(active_parent_id, sizeof(active_parent_id),
                metrics.valid ? metrics.parent_rloc16 : this->candidate_manager_.active_parent_rloc16());
  if (this->active_parent_id_text_sensor_ != nullptr) {
    this->active_parent_id_text_sensor_->publish_state(active_parent_id);
  }

  char standby_parent_id[16];
  format_rloc16(standby_parent_id, sizeof(standby_parent_id), standby_available ? standby.rloc16 : 0xffff);
  if (this->standby_parent_id_text_sensor_ != nullptr) {
    this->standby_parent_id_text_sensor_->publish_state(standby_parent_id);
  }

  if (this->failover_count_sensor_ != nullptr) {
    this->failover_count_sensor_->publish_state(this->failover_controller_.failover_count());
  }

  if (this->degraded_state_binary_sensor_ != nullptr) {
    this->degraded_state_binary_sensor_->publish_state(this->parent_health_monitor_.is_degraded());
  }

  if (this->last_failover_reason_text_sensor_ != nullptr) {
    this->last_failover_reason_text_sensor_->publish_state(this->last_failover_reason_);
  }

  DiagnosticsSnapshot snapshot{};
  snapshot.failover_state = this->failover_controller_.state();
  snapshot.health_state = this->parent_health_monitor_.state();
  snapshot.fail_reason = this->parent_health_monitor_.fail_reason();
  snapshot.standby_valid = standby_available;
  snapshot.active_parent_rloc16 = metrics.valid ? metrics.parent_rloc16 : this->candidate_manager_.active_parent_rloc16();
  snapshot.standby_parent_rloc16 = standby_available ? standby.rloc16 : 0xffff;
  snapshot.failover_count = this->failover_controller_.failover_count();
  snapshot.preferred_attempt_count = this->failover_controller_.preferred_attempt_count();
  snapshot.preferred_success_count = this->failover_controller_.preferred_success_count();
  snapshot.preferred_miss_count = this->failover_controller_.preferred_miss_count();
  snapshot.preferred_target_rloc16 = this->failover_controller_.preferred_last_target_rloc16();
  snapshot.preferred_attached_parent_rloc16 = this->failover_controller_.preferred_last_attached_parent_rloc16();
  snapshot.preferred_outcome = this->failover_controller_.preferred_last_outcome();
  snapshot.preferred_result_state = this->failover_controller_.preferred_last_result_state();

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
