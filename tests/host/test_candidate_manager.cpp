#include <cstdlib>
#include <iostream>

#include "components/biparental_ed/candidate_manager.h"

using esphome::biparental_ed::CandidateManager;
using esphome::biparental_ed::ParentCandidate;

namespace {

bool expect(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << std::endl;
    return false;
  }
  return true;
}

bool test_parent_response_captures_ext_address() {
  CandidateManager manager;
  manager.set_active_parent_rloc16(0x1111);

  const uint8_t ext[8] = {0x02, 0x12, 0x23, 0x34, 0x45, 0x56, 0x67, 0x78};
  manager.observe_parent_response(100, 0x2222, 10, -50, ext);

  ParentCandidate standby{};
  if (!expect(manager.get_standby(101, &standby), "standby candidate should exist")) {
    return false;
  }
  if (!expect(standby.rloc16 == 0x2222, "standby rloc16 should match parent response")) {
    return false;
  }
  if (!expect(standby.has_ext_address, "standby should retain extended address")) {
    return false;
  }
  for (size_t i = 0; i < 8; i++) {
    if (!expect(standby.ext_address[i] == ext[i], "extended address byte mismatch")) {
      return false;
    }
  }
  return true;
}

bool test_failover_continuity_preserves_cached_ext_address() {
  CandidateManager manager;
  const uint8_t active_ext[8] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0x01, 0x02, 0x03};
  const uint8_t standby_ext[8] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80};

  manager.set_active_parent_rloc16(0x1111);
  manager.remember_ext_address(0x1111, active_ext);
  manager.observe_parent_response(100, 0x2222, 10, -50, standby_ext);
  manager.mark_failover_complete(200, 0x2222);

  ParentCandidate standby{};
  if (!expect(manager.get_standby(201, &standby), "continuity standby should exist after failover")) {
    return false;
  }
  if (!expect(standby.rloc16 == 0x1111, "previous active should become standby after failover")) {
    return false;
  }
  if (!expect(standby.has_ext_address, "previous active should keep cached extended address")) {
    return false;
  }
  for (size_t i = 0; i < 8; i++) {
    if (!expect(standby.ext_address[i] == active_ext[i], "cached continuity ext-address byte mismatch")) {
      return false;
    }
  }
  return true;
}

bool test_neighbor_refresh_hydrates_cached_ext_address() {
  CandidateManager manager;
  manager.set_active_parent_rloc16(0x1111);

  const uint8_t ext[8] = {0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97};
  manager.remember_ext_address(0x3333, ext);
  manager.observe_router_neighbor(100, 0x3333, 15, -40, nullptr);

  ParentCandidate standby{};
  if (!expect(manager.get_standby(101, &standby), "neighbor-derived standby should exist")) {
    return false;
  }
  if (!expect(standby.has_ext_address, "neighbor-derived standby should reuse cached extended address")) {
    return false;
  }
  for (size_t i = 0; i < 8; i++) {
    if (!expect(standby.ext_address[i] == ext[i], "neighbor cached ext-address byte mismatch")) {
      return false;
    }
  }
  return true;
}

}  // namespace

int main() {
  if (!test_parent_response_captures_ext_address() || !test_failover_continuity_preserves_cached_ext_address() ||
      !test_neighbor_refresh_hydrates_cached_ext_address()) {
    return EXIT_FAILURE;
  }

  std::cout << "PASS: candidate manager ext address checks" << std::endl;
  return EXIT_SUCCESS;
}
