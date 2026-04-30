#pragma once

#include <array>
#include <cstdint>

namespace esphome {
namespace biparental_ed {

struct ParentCandidate {
  bool valid{false};
  uint16_t rloc16{0xffff};
  std::array<uint8_t, 8> ext_address{};
  bool has_ext_address{false};
  int8_t score{-127};
  int8_t link_margin{0};
  int8_t rssi{-127};
  uint32_t last_refresh_ms{0};
};

class CandidateManager {
 public:
  void set_standby_refresh_interval_ms(uint32_t value) { this->standby_refresh_interval_ms_ = value; }
  void set_replace_hysteresis(int8_t value) { this->replace_hysteresis_ = value; }

  void set_active_parent_rloc16(uint16_t value) { this->active_parent_rloc16_ = value; }
  uint16_t active_parent_rloc16() const { return this->active_parent_rloc16_; }

  void observe_parent_response(uint32_t now_ms, uint16_t rloc16, int8_t link_margin, int8_t rssi,
                               const uint8_t *ext_address = nullptr);
  // Same as observe_parent_response(), but intended for neighbor-table derived candidates.
  void observe_router_neighbor(uint32_t now_ms, uint16_t rloc16, int8_t link_margin, int8_t average_rssi);
  void mark_failover_complete(uint32_t now_ms, uint16_t new_active_rloc16);
  void tick(uint32_t now_ms);

  bool has_standby(uint32_t now_ms) const;
  bool get_standby(uint32_t now_ms, ParentCandidate *candidate) const;
  uint32_t standby_freshness_ms(uint32_t now_ms) const;

 private:
  bool is_stale_(uint32_t now_ms) const;
  static int8_t compute_score_(int8_t link_margin, int8_t rssi);

  uint16_t active_parent_rloc16_{0xffff};
  ParentCandidate standby_candidate_{};

  uint32_t standby_refresh_interval_ms_{60000};
  int8_t replace_hysteresis_{5};
};

}  // namespace biparental_ed
}  // namespace esphome
