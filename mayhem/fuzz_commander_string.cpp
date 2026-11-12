// Commander::run(char*): one command line received outside the library (OSC, radio, ...), executed
// against the same rig as the serial harness. The library locates the end of every value with the
// line sentinel (Commander::isSentinel, the eol char), not with NUL, so the line handed to run()
// carries its eol terminator exactly as the serial path delivers it; the fuzzer owns every byte before it.
#include "harness_common.h"

#include <vector>

Rig *Rig::self = nullptr;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  ScopedRig scoped;
  Rig &rig = *scoped;
  std::vector<char> line(size + 2, 0);
  if (size) memcpy(line.data(), data, size);
  line[size] = rig.commander.eol;
  rig.commander.run(line.data());
  return 0;
}
