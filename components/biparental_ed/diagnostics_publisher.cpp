#include "diagnostics_publisher.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace biparental_ed {

static const char *const TAG = "biparental_ed.diag";

void DiagnosticsPublisher::publish(const DiagnosticsSnapshot &snapshot) {
  if (!this->verbose_) {
    return;
  }

  const uint32_t now_ms = millis();
  const bool periodic_due = this->last_publish_ms_ == 0 || (now_ms - this->last_publish_ms_) >= this->periodic_publish_ms_;
  if (!this->changed_(snapshot) && !periodic_due) {
    return;
  }

  ESP_LOGD(TAG,
           "state=%u health=%u reason=%u standby=%s active=0x%04x (rssi=%d lm=%u age=%ums) standby=0x%04x (score=%d rssi=%d lm=%u fresh=%ums) failovers=%u preferred(a/s/m)=%u/%u/%u",
           static_cast<unsigned>(snapshot.failover_state), static_cast<unsigned>(snapshot.health_state),
           static_cast<unsigned>(snapshot.fail_reason), snapshot.standby_valid ? "yes" : "no",
           snapshot.active_parent_rloc16, snapshot.active_parent_average_rssi,
           static_cast<unsigned>(snapshot.active_parent_link_margin),
           static_cast<unsigned>(snapshot.active_parent_age_ms),
           snapshot.standby_parent_rloc16, snapshot.standby_score, snapshot.standby_rssi,
           static_cast<unsigned>(snapshot.standby_link_margin),
           static_cast<unsigned>(snapshot.standby_freshness_ms),
           static_cast<unsigned>(snapshot.failover_count),
           static_cast<unsigned>(snapshot.preferred_attempt_count),
           static_cast<unsigned>(snapshot.preferred_success_count),
           static_cast<unsigned>(snapshot.preferred_miss_count));

  this->last_snapshot_ = snapshot;
  this->has_last_snapshot_ = true;
  this->last_publish_ms_ = now_ms;
}

bool DiagnosticsPublisher::changed_(const DiagnosticsSnapshot &snapshot) const {
  if (!this->has_last_snapshot_) {
    return true;
  }

  return this->last_snapshot_.failover_state != snapshot.failover_state ||
         this->last_snapshot_.health_state != snapshot.health_state ||
         this->last_snapshot_.fail_reason != snapshot.fail_reason ||
         this->last_snapshot_.standby_valid != snapshot.standby_valid ||
         this->last_snapshot_.active_parent_rloc16 != snapshot.active_parent_rloc16 ||
         this->last_snapshot_.standby_parent_rloc16 != snapshot.standby_parent_rloc16 ||
         this->last_snapshot_.failover_count != snapshot.failover_count ||
         this->last_snapshot_.preferred_attempt_count != snapshot.preferred_attempt_count ||
         this->last_snapshot_.preferred_success_count != snapshot.preferred_success_count ||
         this->last_snapshot_.preferred_miss_count != snapshot.preferred_miss_count ||
         this->last_snapshot_.active_parent_average_rssi != snapshot.active_parent_average_rssi ||
         this->last_snapshot_.active_parent_link_margin != snapshot.active_parent_link_margin ||
         this->last_snapshot_.active_parent_age_ms != snapshot.active_parent_age_ms ||
         this->last_snapshot_.standby_score != snapshot.standby_score ||
         this->last_snapshot_.standby_rssi != snapshot.standby_rssi ||
         this->last_snapshot_.standby_link_margin != snapshot.standby_link_margin ||
         this->last_snapshot_.standby_freshness_ms != snapshot.standby_freshness_ms;
}

}  // namespace biparental_ed
}  // namespace esphome
