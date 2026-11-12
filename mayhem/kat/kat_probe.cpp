// Known-answer probe for the oracle (mayhem/test.sh). Drives the real library through the mock
// Arduino API and prints one `KAT <name> <value>` line per probe; test.sh compares each line with
// an expected value worked out by hand from the library's documented formulas.
#include "harness_common.h"

Rig *Rig::self = nullptr;

static void kat(const char *name, double v) { printf("KAT %s %.4f\n", name, v); }

static void run_line(Rig &rig, const char *text) {
  char buf[128];
  snprintf(buf, sizeof buf, "%s", text);
  rig.commander.run(buf);
}

int main() {
  // PID with a fixed sampling time: P=2, I=100, Ts=10ms, no ramp, no limit.
  {
    PIDController pid(2.0f, 100.0f, 0.0f, NOT_SET, NOT_SET, 0.01f);
    kat("pid_step1", pid(1.0f));      // 2*1 + 100*0.01*0.5*(1+0)       = 2.5
    kat("pid_step2", pid(1.0f));      // 2*1 + (0.5 + 100*0.01*0.5*2)   = 3.5
  }
  // PID output limit and ramp.
  {
    PIDController lim(10.0f, 0.0f, 0.0f, NOT_SET, 4.0f, 0.01f);
    kat("pid_limit", lim(1.0f));      // 10 clamped to the limit        = 4
    PIDController ramp(10.0f, 0.0f, 0.0f, 100.0f, NOT_SET, 0.01f);
    kat("pid_ramp", ramp(1.0f));      // rate-limited to 100*0.01       = 1
  }
  // Low-pass filter with fixed Ts: alpha = Tf/(Tf+Ts) = 0.5.
  {
    LowPassFilter lpf(0.01f, 0.01f);
    kat("lpf_step1", lpf(1.0f));      // 0.5
    kat("lpf_step2", lpf(1.0f));      // 0.75
  }
  // foc_utils.
  kat("normalize_angle", _normalizeAngle(7.0f));           // 7 - 2*pi
  kat("normalize_angle_neg", _normalizeAngle(-1.0f));      // 2*pi - 1
  kat("sin_half_pi", _sin(_PI_2));                         // 1
  kat("cos_pi", _cos(_PI));                                // -1
  kat("electrical_angle", _electricalAngle(1.0f, 7));      // 7
  kat("sqrt_approx", _sqrtApprox(0.25f));                  // fast approximation of 0.5
  kat("atan2_quadrant2", _atan2(1.0f, -1.0f));             // approximation of 3*pi/4

  // BLDCMotor::setPhaseVoltage, Uq=3 Ud=0 angle=0 on a 12 V supply.
  {
    ScopedRig scoped;
  Rig &rig = *scoped;
    rig.motor.foc_modulation = FOCModulationType::SinePWM;
    rig.motor.modulation_centered = true;
    rig.motor.setPhaseVoltage(3.0f, 0.0f, 0.0f);
    kat("sine_centered_a", rig.driver.dc_a);                // 6
    kat("sine_centered_b", rig.driver.dc_b);                // 6 + 3*sqrt(3)/2
    kat("sine_centered_c", rig.driver.dc_c);                // 6 - 3*sqrt(3)/2
    rig.motor.modulation_centered = false;
    rig.motor.setPhaseVoltage(3.0f, 0.0f, 0.0f);
    kat("sine_edge_a", rig.driver.dc_a);                    // 3*sqrt(3)/2
    kat("sine_edge_b", rig.driver.dc_b);                    // 3*sqrt(3)
    kat("sine_edge_c", rig.driver.dc_c);                    // 0
    rig.motor.foc_modulation = FOCModulationType::SpaceVectorPWM;
    rig.motor.modulation_centered = true;
    rig.motor.setPhaseVoltage(3.0f, 0.0f, 0.0f);
    kat("svpwm_a", rig.driver.dc_a);                        // 6
  }

  // Commander text protocol, stream path.
  {
    ScopedRig scoped;
  Rig &rig = *scoped;
    const char *script = "PP9\nPI8.5\nFF0.25\nS-2.5\nX1.5\nX2\nMK120\nMLU5\nMC1\nT3 0.8\n";
    ByteStream in((const uint8_t *)script, strlen(script));
    rig.commander.run(in, '\n');
    kat("cmd_pid_p", rig.pid.P);                            // 9
    kat("cmd_pid_i", rig.pid.I);                            // 8.5
    kat("cmd_lpf_tf", rig.lpf.Tf);                          // 0.25
    kat("cmd_scalar", rig.scalar);                          // -2.5
    kat("cmd_custom_sum", rig.custom_sum);                  // 3.5
    kat("cmd_motor_kv", rig.motor.KV_rating);               // 120
    kat("cmd_motor_voltage_limit", rig.motor.voltage_limit);  // 0.8 (set by the target's 2nd value)
    kat("cmd_motion_type", (double)rig.motor.controller);   // velocity = 1
    kat("cmd_target", rig.motor.target);                    // 3
  }
  // Commander text protocol, string path: a get leaves the value alone, an unknown id is ignored.
  {
    ScopedRig scoped;
  Rig &rig = *scoped;
    run_line(rig, "S7\n");
    run_line(rig, "S\n");
    run_line(rig, "Z99\n");
    kat("cmd_get_keeps_value", rig.scalar);                 // 7
    run_line(rig, "PD-0.5\n");
    kat("cmd_pid_d", rig.pid.D);                            // -0.5
    run_line(rig, "MWT1\n");
    kat("cmd_pwm_mod_type", (double)rig.motor.foc_modulation);  // SpaceVectorPWM = 1
  }

  // Torque control in voltage mode: move() latches the target, loopFOC() turns it into the q voltage.
  {
    ScopedRig scoped;
  Rig &rig = *scoped;
    rig.motor.controller = MotionControlType::torque;
    rig.motor.torque_controller = TorqueControlType::voltage;
    rig.motor.sensor_direction = Direction::CW;
    rig.motor.zero_electric_angle = 0.0f;
    rig.motor.enable();
    rig.motor.move(2.0f);
    rig.motor.loopFOC();
    kat("move_torque_target", rig.motor.target);            // 2
    kat("move_torque_voltage_q", rig.motor.voltage.q);      // 2
  }
  return 0;
}
