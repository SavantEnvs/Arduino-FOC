// Shared fixture for the fuzz harnesses and the KAT probe: a BLDCMotor wired to a mock driver and
// a mock position sensor, with a Commander exposing the usual SimpleFOC command interface.
// Everything is built fresh per input, so an input's result depends only on that input.
#ifndef MAYHEM_HARNESS_COMMON_H
#define MAYHEM_HARNESS_COMMON_H

#include "Arduino.h"
#include <new>
#include "BLDCMotor.h"
#include "communication/Commander.h"
#include "common/pid.h"
#include "common/lowpass_filter.h"

class MockDriver : public BLDCDriver {
public:
  int init() override { initialized = true; return 1; }
  void enable() override {}
  void disable() override {}
  void setPwm(float a, float b, float c) override { dc_a = a; dc_b = b; dc_c = c; }
  void setPhaseState(PhaseState, PhaseState, PhaseState) override {}
};

// A sensor that keeps advancing; this lets initFOC's direction detection run to completion.
class MockSensor : public Sensor {
public:
  float angle = 0.0f;
  float step = 0.001f;
protected:
  float getSensorAngle() override { angle += step; return angle; }
};

class ByteStream : public Stream {
public:
  ByteStream(const uint8_t *d, size_t n) : data_(d), size_(n) {}
  int available() override { return (int)(size_ - pos_); }
  int read() override { return pos_ < size_ ? data_[pos_++] : -1; }
  int peek() override { return pos_ < size_ ? data_[pos_] : -1; }
private:
  const uint8_t *data_;
  size_t size_;
  size_t pos_ = 0;
};

struct Rig {
  MockDriver driver;
  MockSensor sensor;
  BLDCMotor motor;
  PIDController pid;
  LowPassFilter lpf;
  float scalar = 1.0f;
  float custom_sum = 0.0f;
  Commander commander;

  static Rig *self;

  Rig() : motor(7), pid(1.0f, 2.0f, 0.0f, 100.0f, 12.0f), lpf(0.01f) {
    mock_reset_clock();
    driver.voltage_power_supply = 12.0f;
    driver.voltage_limit = 12.0f;
    driver.init();
    motor.linkDriver(&driver);
    motor.linkSensor(&sensor);
    motor.voltage_limit = 12.0f;
    motor.init();
    self = this;
    commander.add('M', onMotor, "motor");
    commander.add('P', onPid, "pid");
    commander.add('F', onLpf, "lpf");
    commander.add('S', onScalar, "scalar");
    commander.add('T', onTarget, "target");
    commander.add('X', onCustom, "custom");
  }
  ~Rig() { self = nullptr; }

  static void onMotor(char *c) { self->commander.motor(&self->motor, c); }
  static void onPid(char *c) { self->commander.pid(&self->pid, c); }
  static void onLpf(char *c) { self->commander.lpf(&self->lpf, c); }
  static void onScalar(char *c) { self->commander.scalar(&self->scalar, c); }
  static void onTarget(char *c) { self->commander.target(&self->motor, c); }
  static void onCustom(char *c) { self->custom_sum += (float)atof(c); }
};

// The library leaves several members (DQ voltages/currents, setpoints) without initializers because on a
// microcontroller its motors are globals and so start zeroed. Build the rig in zeroed static storage to
// match, which also keeps every input independent of stack garbage left by the previous one.
class ScopedRig {
public:
  ScopedRig() {
    memset(storage(), 0, sizeof(Rig));
    rig_ = new (storage()) Rig();
  }
  ~ScopedRig() { rig_->~Rig(); }
  ScopedRig(const ScopedRig &) = delete;
  ScopedRig &operator=(const ScopedRig &) = delete;
  Rig &operator*() { return *rig_; }
  Rig *operator->() { return rig_; }
private:
  static void *storage() {
    alignas(Rig) static unsigned char buf[sizeof(Rig)];
    return buf;
  }
  Rig *rig_;
};

#endif
