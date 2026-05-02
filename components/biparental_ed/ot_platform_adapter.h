#pragma once

#include <cstdint>

#include "parent_health_monitor.h"

namespace esphome {
namespace biparental_ed {

enum class PreferredFailoverRequestMode : uint8_t {
  REJECT = 0,
  ALREADY_ATTACHED = 1,
  SEARCH_WHILE_ATTACHED = 2,
  BECOME_CHILD = 3,
  DETACH_AND_REATTACH = 4,
};

enum class PreferredFailoverRole : uint8_t {
  DISABLED = 0,
  DETACHED = 1,
  CHILD = 2,
  OTHER = 3,
};

inline PreferredFailoverRequestMode choose_preferred_failover_request_mode(PreferredFailoverRole role,
                                                                           bool already_attached_to_preferred) {
  switch (role) {
    case PreferredFailoverRole::DISABLED:
      return PreferredFailoverRequestMode::REJECT;

    case PreferredFailoverRole::CHILD:
      return already_attached_to_preferred ? PreferredFailoverRequestMode::ALREADY_ATTACHED
                                           : PreferredFailoverRequestMode::SEARCH_WHILE_ATTACHED;

    case PreferredFailoverRole::DETACHED:
      return PreferredFailoverRequestMode::BECOME_CHILD;

    case PreferredFailoverRole::OTHER:
    default:
      return PreferredFailoverRequestMode::DETACH_AND_REATTACH;
  }
}

class OpenThreadPlatformAdapter {
 public:
  virtual ~OpenThreadPlatformAdapter() = default;

  virtual bool is_attached_as_child() const = 0;
  virtual bool read_parent_metrics(ParentMetrics *metrics) = 0;
  // Iterates router neighbors (excluding children) as candidates for standby selection.
  // Callback must be fast and must not call back into OpenThread.
  struct RouterNeighborInfo {
    uint16_t rloc16{0xffff};
    int8_t average_rssi{-127};
    int8_t last_rssi{-127};
    uint8_t link_margin{0};
    uint32_t age_ms{0};
    uint8_t ext_address[8]{};
    bool has_ext_address{false};
  };
  using RouterNeighborCallback = void (*)(void *context, const RouterNeighborInfo &info);
  virtual bool read_router_neighbors(RouterNeighborCallback cb, void *context) = 0;
  virtual bool request_parent_search() = 0;
  virtual bool request_failover_to_standby_target(uint16_t preferred_rloc16, const uint8_t *preferred_ext_address) = 0;
  virtual bool request_failover_to_preferred(uint16_t preferred_rloc16) = 0;
  virtual bool request_failover_generic() = 0;
};

class NoopOpenThreadPlatformAdapter : public OpenThreadPlatformAdapter {
 public:
  bool is_attached_as_child() const override { return false; }

  bool read_parent_metrics(ParentMetrics *metrics) override {
    if (metrics == nullptr) {
      return false;
    }

    metrics->valid = false;
    metrics->average_rssi = -127;
    metrics->last_rssi = -127;
    metrics->parent_rloc16 = 0xffff;
    metrics->has_parent_ext_address = false;
    metrics->parent_link_margin = 0;
    metrics->parent_age_ms = 0;
    metrics->supervision_ok = false;
    metrics->control_plane_error_count = 0;
    metrics->last_parent_rx_ms = 0;
    return false;
  }

  bool read_router_neighbors(RouterNeighborCallback, void *) override { return false; }

  bool request_parent_search() override { return false; }
  bool request_failover_to_standby_target(uint16_t, const uint8_t *) override { return false; }
  bool request_failover_to_preferred(uint16_t) override { return false; }
  bool request_failover_generic() override { return false; }
};

class EspHomeOpenThreadPlatformAdapter : public OpenThreadPlatformAdapter {
 public:
  bool is_attached_as_child() const override;
  bool read_parent_metrics(ParentMetrics *metrics) override;
  bool read_router_neighbors(RouterNeighborCallback cb, void *context) override;
  bool request_parent_search() override;
  bool request_failover_to_standby_target(uint16_t preferred_rloc16, const uint8_t *preferred_ext_address) override;
  bool request_failover_to_preferred(uint16_t preferred_rloc16) override;
  bool request_failover_generic() override;
};

}  // namespace biparental_ed
}  // namespace esphome
