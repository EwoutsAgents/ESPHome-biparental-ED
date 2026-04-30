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

}  // namespace

int main() {
  if (!test_parent_response_captures_ext_address()) {
    return EXIT_FAILURE;
  }

  std::cout << "PASS: candidate manager ext address checks" << std::endl;
  return EXIT_SUCCESS;
}
