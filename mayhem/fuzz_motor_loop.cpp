// The motion/FOC control loop: the fuzzer picks the motor configuration (control types, modulation,
// limits, gains) and drives loopFOC()/move() with an arbitrary sequence of targets and sensor readings
// through the mock driver, covering the PID/LPF/open-loop/PWM-modulation math in the library.
// Include before the Arduino mock: its min/max macros break the C++ standard headers.
#include <fuzzer/FuzzedDataProvider.h>

#include "harness_common.h"

Rig *Rig::self = nullptr;

// Values are drawn from the ranges a real motor setup uses; non-finite or astronomically large settings
// only exercise float-to-int conversion UB in foc_utils, which is not what this target is for.
static float range(FuzzedDataProvider &fdp, float lo, float hi) {
  return fdp.ConsumeFloatingPointInRange<float>(lo, hi);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  FuzzedDataProvider fdp(data, size);
  ScopedRig scoped;
  Rig &rig = *scoped;
  BLDCMotor &m = rig.motor;

  m.controller = (MotionControlType)fdp.ConsumeIntegralInRange<int>(0, 6);
  m.torque_controller = (TorqueControlType)fdp.ConsumeIntegralInRange<int>(0, 3);
  m.foc_modulation = (FOCModulationType)fdp.ConsumeIntegralInRange<int>(0, 3);
  m.modulation_centered = fdp.ConsumeBool();
  m.sensor_direction = fdp.ConsumeBool() ? Direction::CW : Direction::CCW;
  m.zero_electric_angle = range(fdp, 0.0f, 6.3f);
  m.voltage_limit = range(fdp, 0.0f, 24.0f);
  m.current_limit = range(fdp, 0.0f, 20.0f);
  m.velocity_limit = range(fdp, 0.0f, 500.0f);
  m.phase_resistance = range(fdp, 0.0f, 20.0f);
  m.KV_rating = range(fdp, 0.0f, 1000.0f);
  m.PID_velocity.P = range(fdp, 0.0f, 10.0f);
  m.PID_velocity.I = range(fdp, 0.0f, 100.0f);
  m.P_angle.P = range(fdp, 0.0f, 50.0f);
  m.motion_downsample = fdp.ConsumeIntegralInRange<int>(0, 3);
  rig.sensor.step = range(fdp, -0.5f, 0.5f);
  m.enable();

  int iterations = fdp.ConsumeIntegralInRange<int>(1, 32);
  for (int i = 0; i < iterations; i++) {
    m.loopFOC();
    m.move(range(fdp, -1000.0f, 1000.0f));
  }
  return 0;
}
