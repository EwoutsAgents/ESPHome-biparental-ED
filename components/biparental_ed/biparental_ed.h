#pragma once

#include "esphome/core/component.h"

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

 protected:
  void apply_runtime_configuration_();
  void maybe_issue_failover_action_(uint32_t now_ms, const FailoverAction &action, const ParentCandidate &standby,
                                    bool standby_available);
  void publish_diagnostics_(bool standby_available, const ParentCandidate &standby);

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

  ParentHealthMonitor parent_health_monitor_{};
  CandidateManager candidate_manager_{};
  FailoverController failover_controller_{};
  DiagnosticsPublisher diagnostics_publisher_{};

  NoopOpenThreadPlatformAdapter noop_ot_adapter_{};
  OpenThreadPlatformAdapter *ot_adapter_{&this->noop_ot_adapter_};
};

}  // namespace biparental_ed
}  // namespace esphome
