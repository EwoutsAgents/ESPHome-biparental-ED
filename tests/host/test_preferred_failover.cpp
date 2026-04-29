#include <cstdlib>
#include <iostream>

#include "components/biparental_ed/failover_controller.h"
#include "components/biparental_ed/ot_platform_adapter.h"

using esphome::biparental_ed::FailoverActionReason;
using esphome::biparental_ed::FailoverActionType;
using esphome::biparental_ed::FailoverController;
using esphome::biparental_ed::FailoverState;
using esphome::biparental_ed::PreferredFailoverRequestMode;
using esphome::biparental_ed::PreferredFailoverRole;
using esphome::biparental_ed::PreferredReattachOutcome;
using esphome::biparental_ed::choose_preferred_failover_request_mode;

namespace {

bool expect(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << std::endl;
    return false;
  }
  return true;
}

bool test_preferred_request_stays_attached() {
  const auto mode = choose_preferred_failover_request_mode(PreferredFailoverRole::CHILD, false);
  return expect(mode == PreferredFailoverRequestMode::SEARCH_WHILE_ATTACHED,
                "attached child should use search-without-detach for preferred request");
}

bool test_preferred_timeout_falls_back_to_generic() {
  FailoverController controller;
  controller.set_hold_down_time_ms(0);
  controller.set_failover_eligible_delay_ms(0);
  controller.set_preferred_reattach_timeout_ms(50);

  (void) controller.evaluate(0, false, false, false, false, 0xffff, 0xffff);
  (void) controller.evaluate(1, true, true, false, false, 0x1111, 0x2222);
  (void) controller.evaluate(2, true, true, true, false, 0x1111, 0x2222);
  const auto preferred = controller.evaluate(3, true, true, true, false, 0x1111, 0x2222);
  if (!expect(preferred.type == FailoverActionType::TRIGGER_PREFERRED_REATTACH,
              "health failure with standby should trigger preferred path")) {
    return false;
  }

  const auto timeout = controller.evaluate(60, false, true, true, false, 0xffff, 0x2222);
  return expect(timeout.type == FailoverActionType::TRIGGER_GENERIC_REATTACH,
                "preferred timeout should trigger generic fallback") &&
         expect(timeout.reason == FailoverActionReason::PREFERRED_TIMEOUT,
                "preferred timeout should be classified as PREFERRED_TIMEOUT") &&
         expect(controller.state() == FailoverState::REATTACHING_GENERIC,
                "controller should move to generic reattach state after preferred timeout") &&
         expect(controller.preferred_last_outcome() == PreferredReattachOutcome::TIMEOUT,
                "preferred timeout should be recorded as timeout outcome");
}

bool test_preferred_success_requires_exact_parent_match() {
  FailoverController controller;
  controller.set_hold_down_time_ms(0);
  controller.set_failover_eligible_delay_ms(0);
  controller.set_preferred_reattach_timeout_ms(500);

  (void) controller.evaluate(0, false, false, false, false, 0xffff, 0xffff);
  (void) controller.evaluate(1, true, true, false, false, 0x1111, 0x2222);
  (void) controller.evaluate(2, true, true, true, false, 0x1111, 0x2222);
  (void) controller.evaluate(3, true, true, true, false, 0x1111, 0x2222);

  const auto miss = controller.evaluate(4, true, true, true, false, 0x3333, 0x2222);
  if (!expect(miss.type == FailoverActionType::TRIGGER_GENERIC_REATTACH,
              "attach to non-target parent should be treated as miss")) {
    return false;
  }
  if (!expect(miss.reason == FailoverActionReason::PREFERRED_MISS,
              "attach to non-target parent should emit preferred miss")) {
    return false;
  }
  if (!expect(controller.preferred_success_count() == 0,
              "preferred success count must stay zero on non-target attach")) {
    return false;
  }

  FailoverController success_controller;
  success_controller.set_hold_down_time_ms(0);
  success_controller.set_failover_eligible_delay_ms(0);
  success_controller.set_preferred_reattach_timeout_ms(500);
  (void) success_controller.evaluate(0, false, false, false, false, 0xffff, 0xffff);
  (void) success_controller.evaluate(1, true, true, false, false, 0x1111, 0x2222);
  (void) success_controller.evaluate(2, true, true, true, false, 0x1111, 0x2222);
  (void) success_controller.evaluate(3, true, true, true, false, 0x1111, 0x2222);
  const auto success = success_controller.evaluate(4, true, true, true, false, 0x2222, 0x2222);

  return expect(success.type == FailoverActionType::NONE,
                "exact target attach should not trigger generic fallback") &&
         expect(success_controller.state() == FailoverState::POST_FAILOVER_STABILIZE,
                "exact target attach should enter post-failover stabilize") &&
         expect(success_controller.preferred_success_count() == 1,
                "preferred success count should increment only on exact target attach") &&
         expect(success_controller.preferred_last_outcome() == PreferredReattachOutcome::SUCCESS,
                "exact target attach should be recorded as success");
}

}  // namespace

int main() {
  bool ok = true;
  ok = test_preferred_request_stays_attached() && ok;
  ok = test_preferred_timeout_falls_back_to_generic() && ok;
  ok = test_preferred_success_requires_exact_parent_match() && ok;

  if (!ok) {
    return EXIT_FAILURE;
  }

  std::cout << "PASS: preferred failover host checks" << std::endl;
  return EXIT_SUCCESS;
}
