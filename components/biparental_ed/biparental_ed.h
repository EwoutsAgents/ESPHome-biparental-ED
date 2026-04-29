#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"

#include <string>

#include "candidate_manager.h"
#include "diagnostics_publisher.h"
#include "failover_controller.h"
#include "ot_platform_adapter.h"
#include "parent_health_monitor.h"

namespace esphome {
namespace biparental_ed {

class BiparentalEDComponent : public PollingComponent {
 public:
  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;
  void observe_parent_response_(uint16_t rloc16, int8_t rssi, uint8_t link_quality_3, uint8_t link_quality_2,
                                uint8_t link_quality_1, bool is_attached);

  void set_degraded_rssi_threshold(int value) { this->degraded_rssi_threshold_ = value; }
  void set_hard_failure_timeout_ms(uint32_t value) { this->hard_failure_timeout_ms_ = value; }
  void set_degrade_persist_time_ms(uint32_t value) { this->degrade_persist_time_ms_ = value; }
  void set_hold_down_time_ms(uint32_t value) { this->hold_down_time_ms_ = value; }
  void set_standby_refresh_interval_ms(uint32_t value) { this->standby_refresh_interval_ms_ = value; }
  void set_failover_eligible_delay_ms(uint32_t value) { this->failover_eligible_delay_ms_ = value; }
  void set_post_failover_stabilize_time_ms(uint32_t value) { this->post_failover_stabilize_time_ms_ = value; }
  void set_preferred_reattach_timeout_ms(uint32_t value) { this->preferred_reattach_timeout_ms_ = value; }
  void set_enable_parent_response_callback(bool value) { this->enable_parent_response_callback_ = value; }
  void set_control_plane_error_threshold(uint32_t value) { this->control_plane_error_threshold_ = value; }
  void set_standby_replace_hysteresis(int value) { this->standby_replace_hysteresis_ = value; }

  void set_neighbor_scan_interval_ms(uint32_t value) { this->neighbor_scan_interval_ms_ = value; }
  void set_neighbor_max_age_ms(uint32_t value) { this->neighbor_max_age_ms_ = value; }
  void set_parent_search_refresh_interval_ms(uint32_t value) { this->parent_search_refresh_interval_ms_ = value; }
  void set_runtime_debug_enabled(bool value) { this->runtime_debug_enabled_ = value; }
  void set_runtime_debug_interval_ms(uint32_t value) { this->runtime_debug_interval_ms_ = value; }
  void set_verbose_diagnostics(bool value) { this->verbose_diagnostics_ = value; }
  void set_diagnostics_publish_interval_ms(uint32_t value) { this->diagnostics_publish_interval_ms_ = value; }

  void set_active_parent_id_text_sensor(text_sensor::TextSensor *sensor) { this->active_parent_id_text_sensor_ = sensor; }
  void set_standby_parent_id_text_sensor(text_sensor::TextSensor *sensor) { this->standby_parent_id_text_sensor_ = sensor; }
  void set_failover_count_sensor(sensor::Sensor *sensor) { this->failover_count_sensor_ = sensor; }
  void set_degraded_state_binary_sensor(binary_sensor::BinarySensor *sensor) { this->degraded_state_binary_sensor_ = sensor; }
  void set_last_failover_reason_text_sensor(text_sensor::TextSensor *sensor) { this->last_failover_reason_text_sensor_ = sensor; }

 protected:
  void apply_runtime_configuration_();
  void maybe_scan_neighbors_(uint32_t now_ms);
  void maybe_trigger_parent_search_refresh_(uint32_t now_ms);
  void maybe_log_preferred_outcome_();
  void maybe_issue_failover_action_(uint32_t now_ms, const FailoverAction &action, const ParentCandidate &standby,
                                    bool standby_available);
  void publish_diagnostics_(const ParentMetrics &metrics, bool standby_available, const ParentCandidate &standby);

  int degraded_rssi_threshold_{-70};
  uint32_t hard_failure_timeout_ms_{15000};
  uint32_t degrade_persist_time_ms_{10000};
  uint32_t hold_down_time_ms_{30000};
  uint32_t standby_refresh_interval_ms_{60000};
  uint32_t failover_eligible_delay_ms_{10000};
  uint32_t post_failover_stabilize_time_ms_{10000};
  uint32_t preferred_reattach_timeout_ms_{5000};
  bool enable_parent_response_callback_{true};
  uint32_t control_plane_error_threshold_{3};
  int standby_replace_hysteresis_{5};

  uint32_t neighbor_scan_interval_ms_{10000};
  uint32_t neighbor_max_age_ms_{60000};
  // If non-zero, triggers OpenThread's better-parent search periodically.
  // NOTE: this may cause an automatic parent switch by the stack.
  uint32_t parent_search_refresh_interval_ms_{0};

  uint32_t last_neighbor_scan_ms_{0};
  uint32_t last_parent_search_ms_{0};
  bool parent_response_callback_registered_{false};
  uint32_t last_logged_preferred_outcome_event_count_{0};
  bool runtime_debug_enabled_{true};
  uint32_t runtime_debug_interval_ms_{5000};
  bool verbose_diagnostics_{true};
  uint32_t diagnostics_publish_interval_ms_{5000};

  text_sensor::TextSensor *active_parent_id_text_sensor_{nullptr};
  text_sensor::TextSensor *standby_parent_id_text_sensor_{nullptr};
  sensor::Sensor *failover_count_sensor_{nullptr};
  binary_sensor::BinarySensor *degraded_state_binary_sensor_{nullptr};
  text_sensor::TextSensor *last_failover_reason_text_sensor_{nullptr};
  std::string last_failover_reason_{"none"};

  ParentHealthMonitor parent_health_monitor_{};
  CandidateManager candidate_manager_{};
  FailoverController failover_controller_{};
  DiagnosticsPublisher diagnostics_publisher_{};

  NoopOpenThreadPlatformAdapter noop_ot_adapter_{};
  OpenThreadPlatformAdapter *ot_adapter_{&this->noop_ot_adapter_};
  EspHomeOpenThreadPlatformAdapter esphome_ot_adapter_{};
};

}  // namespace biparental_ed
}  // namespace esphome
