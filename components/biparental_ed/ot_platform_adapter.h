#pragma once

#include <cstdint>

#include "parent_health_monitor.h"

namespace esphome {
namespace biparental_ed {

class OpenThreadPlatformAdapter {
 public:
  virtual ~OpenThreadPlatformAdapter() = default;

  virtual bool is_attached_as_child() const = 0;
  virtual bool read_parent_metrics(ParentMetrics *metrics) = 0;
  virtual bool request_parent_search() = 0;
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
    metrics->supervision_ok = false;
    metrics->control_plane_error_count = 0;
    metrics->last_parent_rx_ms = 0;
    return false;
  }

  bool request_parent_search() override { return false; }
  bool request_failover_to_preferred(uint16_t) override { return false; }
  bool request_failover_generic() override { return false; }
};

}  // namespace biparental_ed
}  // namespace esphome
