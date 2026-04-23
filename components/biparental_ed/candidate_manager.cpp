#include "candidate_manager.h"

namespace esphome {
namespace biparental_ed {

void CandidateManager::observe_parent_response(uint32_t now_ms, uint16_t rloc16, int8_t link_margin, int8_t rssi) {
  if (rloc16 == this->active_parent_rloc16_) {
    return;
  }

  const int8_t score = static_cast<int8_t>(link_margin + (rssi / 2));
  const bool should_replace = !this->standby_candidate_.valid || this->is_stale_(now_ms) ||
                              score >= (this->standby_candidate_.score + this->replace_hysteresis_);

  if (!should_replace) {
    return;
  }

  this->standby_candidate_.valid = true;
  this->standby_candidate_.rloc16 = rloc16;
  this->standby_candidate_.link_margin = link_margin;
  this->standby_candidate_.rssi = rssi;
  this->standby_candidate_.score = score;
  this->standby_candidate_.last_refresh_ms = now_ms;
}

void CandidateManager::mark_failover_complete(uint32_t now_ms, uint16_t new_active_rloc16) {
  this->active_parent_rloc16_ = new_active_rloc16;

  if (this->standby_candidate_.valid && this->standby_candidate_.rloc16 == new_active_rloc16) {
    this->standby_candidate_.valid = false;
    this->standby_candidate_.last_refresh_ms = now_ms;
  }
}

void CandidateManager::tick(uint32_t now_ms) {
  if (this->is_stale_(now_ms)) {
    this->standby_candidate_.valid = false;
  }
}

bool CandidateManager::has_standby(uint32_t now_ms) const { return this->standby_candidate_.valid && !this->is_stale_(now_ms); }

bool CandidateManager::get_standby(uint32_t now_ms, ParentCandidate *candidate) const {
  if (!this->has_standby(now_ms) || candidate == nullptr) {
    return false;
  }

  *candidate = this->standby_candidate_;
  return true;
}

bool CandidateManager::is_stale_(uint32_t now_ms) const {
  if (!this->standby_candidate_.valid) {
    return true;
  }
  return (now_ms - this->standby_candidate_.last_refresh_ms) >= this->standby_refresh_interval_ms_;
}

}  // namespace biparental_ed
}  // namespace esphome
