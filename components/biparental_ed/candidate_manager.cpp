#include "candidate_manager.h"

namespace esphome {
namespace biparental_ed {

int8_t CandidateManager::compute_score_(int8_t link_margin, int8_t rssi) {
  // Heuristic score, intended to be stable and cheap:
  // - Link margin dominates (0..255 range in OT)
  // - RSSI contributes but is bounded.
  return static_cast<int8_t>(link_margin + (rssi / 2));
}

void CandidateManager::observe_parent_response(uint32_t now_ms, uint16_t rloc16, int8_t link_margin, int8_t rssi,
                                               const uint8_t *ext_address) {
  if (rloc16 == this->active_parent_rloc16_) {
    return;
  }

  const int8_t score = compute_score_(link_margin, rssi);

  // Always refresh metrics/timestamp for the current standby.
  if (this->standby_candidate_.valid && rloc16 == this->standby_candidate_.rloc16) {
    this->standby_candidate_.link_margin = link_margin;
    this->standby_candidate_.rssi = rssi;
    this->standby_candidate_.score = score;
    this->standby_candidate_.last_refresh_ms = now_ms;
    if (ext_address != nullptr) {
      for (std::size_t i = 0; i < this->standby_candidate_.ext_address.size(); i++) {
        this->standby_candidate_.ext_address[i] = ext_address[i];
      }
      this->standby_candidate_.has_ext_address = true;
    }
    return;
  }

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
  this->standby_candidate_.has_ext_address = ext_address != nullptr;
  if (ext_address != nullptr) {
    for (std::size_t i = 0; i < this->standby_candidate_.ext_address.size(); i++) {
      this->standby_candidate_.ext_address[i] = ext_address[i];
    }
  }
}

void CandidateManager::observe_router_neighbor(uint32_t now_ms, uint16_t rloc16, int8_t link_margin,
                                               int8_t average_rssi) {
  // For neighbor-table derived candidates we use average RSSI.
  this->observe_parent_response(now_ms, rloc16, link_margin, average_rssi);
}

void CandidateManager::mark_failover_complete(uint32_t now_ms, uint16_t new_active_rloc16) {
  const uint16_t previous_active_rloc16 = this->active_parent_rloc16_;
  this->active_parent_rloc16_ = new_active_rloc16;

  if (this->standby_candidate_.valid && this->standby_candidate_.rloc16 == new_active_rloc16) {
    this->standby_candidate_.valid = false;
    this->standby_candidate_.last_refresh_ms = now_ms;
  }

  // Best-effort standby continuity: when active parent changes, keep the previous active
  // as a standby candidate so preferred failover has a concrete target.
  if (previous_active_rloc16 != 0xffff && previous_active_rloc16 != 0xfffe &&
      previous_active_rloc16 != new_active_rloc16) {
    this->standby_candidate_.valid = true;
    this->standby_candidate_.rloc16 = previous_active_rloc16;
    this->standby_candidate_.link_margin = 0;
    this->standby_candidate_.rssi = -127;
    this->standby_candidate_.score = compute_score_(this->standby_candidate_.link_margin, this->standby_candidate_.rssi);
    this->standby_candidate_.last_refresh_ms = now_ms;
    this->standby_candidate_.has_ext_address = false;
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

uint32_t CandidateManager::standby_freshness_ms(uint32_t now_ms) const {
  if (!this->standby_candidate_.valid) {
    return UINT32_MAX;
  }
  return now_ms - this->standby_candidate_.last_refresh_ms;
}

bool CandidateManager::is_stale_(uint32_t now_ms) const {
  if (!this->standby_candidate_.valid) {
    return true;
  }
  return (now_ms - this->standby_candidate_.last_refresh_ms) >= this->standby_refresh_interval_ms_;
}

}  // namespace biparental_ed
}  // namespace esphome
