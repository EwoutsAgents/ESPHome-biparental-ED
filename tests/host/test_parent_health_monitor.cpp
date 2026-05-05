#include <cstdlib>
#include <iostream>

#include "components/biparental_ed/parent_health_monitor.h"

using esphome::biparental_ed::ParentFailReason;
using esphome::biparental_ed::ParentHealthMonitor;
using esphome::biparental_ed::ParentHealthState;
using esphome::biparental_ed::ParentMetrics;

namespace {

bool expect(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << std::endl;
    return false;
  }
  return true;
}

bool test_inferred_timeout_does_not_fail() {
  ParentHealthMonitor monitor;
  ParentMetrics metrics{};
  metrics.valid = true;
  metrics.average_rssi = -40;
  metrics.parent_rloc16 = 0x1234;
  metrics.supervision_ok = true;
  metrics.last_parent_rx_ms = 0;
  metrics.last_parent_rx_is_inferred = true;

  monitor.evaluate(16000, metrics);

  return expect(monitor.state() == ParentHealthState::HEALTHY,
                "inferred parent age timeout should not force failed state") &&
         expect(monitor.fail_reason() == ParentFailReason::NONE,
                "inferred parent age timeout should not set hard-timeout reason");
}

bool test_supervision_failure_still_fails() {
  ParentHealthMonitor monitor;
  ParentMetrics metrics{};
  metrics.valid = true;
  metrics.average_rssi = -40;
  metrics.parent_rloc16 = 0x1234;
  metrics.supervision_ok = false;
  metrics.last_parent_rx_ms = 1000;
  metrics.last_parent_rx_is_inferred = true;

  monitor.evaluate(2000, metrics);

  return expect(monitor.state() == ParentHealthState::FAILED,
                "supervision failure should still force failed state") &&
         expect(monitor.fail_reason() == ParentFailReason::HARD_TIMEOUT,
                "supervision failure should still report hard-timeout reason");
}

bool test_invalid_metrics_still_fail() {
  ParentHealthMonitor monitor;
  ParentMetrics metrics{};
  metrics.valid = false;

  monitor.evaluate(1000, metrics);

  return expect(monitor.state() == ParentHealthState::FAILED,
                "invalid metrics should still force failed state") &&
         expect(monitor.fail_reason() == ParentFailReason::HARD_TIMEOUT,
                "invalid metrics should still report hard-timeout reason");
}

}  // namespace

int main() {
  bool ok = true;
  ok = test_inferred_timeout_does_not_fail() && ok;
  ok = test_supervision_failure_still_fails() && ok;
  ok = test_invalid_metrics_still_fail() && ok;

  if (!ok) {
    return EXIT_FAILURE;
  }

  std::cout << "PASS: parent health monitor checks" << std::endl;
  return EXIT_SUCCESS;
}
