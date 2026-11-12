// Commander's serial path: bytes arrive on a Stream and every eol-terminated line is executed
// against a motor, PID, low-pass filter, scalar and plain callbacks (Commander::run(Stream&)).
#include "harness_common.h"

Rig *Rig::self = nullptr;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  ScopedRig scoped;
  Rig &rig = *scoped;
  rig.commander.echo = true;
  ByteStream stream(data, size);
  rig.commander.run(stream, '\n');
  return 0;
}
