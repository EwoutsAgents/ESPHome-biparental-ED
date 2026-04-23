#include "diagnostics_publisher.h"

#include "esphome/core/log.h"

namespace esphome {
namespace biparental_ed {

static const char *const TAG = "biparental_ed.diag";

void DiagnosticsPublisher::publish(const DiagnosticsSnapshot &snapshot) {
  if (!this->verbose_ || !this->changed_(snapshot)) {
    return;
  }

  ESP_LOGD(TAG,
           "state=%u health=%u reason=%u standby=%s active=0x%04x standby_rloc=0x%04x failovers=%u",
           static_cast<unsigned>(snapshot.failover_state), static_cast<unsigned>(snapshot.health_state),
           static_cast<unsigned>(snapshot.fail_reason), snapshot.standby_valid ? "yes" : "no",
           snapshot.active_parent_rloc16, snapshot.standby_parent_rloc16,
           static_cast<unsigned>(snapshot.failover_count));

  this->last_snapshot_ = snapshot;
  this->has_last_snapshot_ = true;
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
         this->last_snapshot_.failover_count != snapshot.failover_count;
}

}  // namespace biparental_ed
}  // namespace esphome
