// Minimal host-side stand-in for the Arduino core API that SimpleFOC's hardware-independent
// sources use (Print/Stream, F(), timing, a few macros). Output is discarded; time is a
// deterministic virtual clock (see arduino_mock.cpp) so results never depend on host speed.
#ifndef MAYHEM_ARDUINO_MOCK_H
#define MAYHEM_ARDUINO_MOCK_H

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <inttypes.h>
// The Arduino min/max macros clash with the C++ library headers, so pull those in first.
#include <algorithm>
#include <vector>

typedef bool boolean;
typedef uint8_t byte;

#define PROGMEM
#define PSTR(s) (s)

class __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper *>(s))

#define HIGH 0x1
#define LOW 0x0
#define INPUT 0x0
#define OUTPUT 0x1
#define INPUT_PULLUP 0x2
#define DEG_TO_RAD 0.017453292519943295769236907684886
#define RAD_TO_DEG 57.295779513082320876798154814105

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
#define radians(deg) ((deg) * DEG_TO_RAD)
#define degrees(rad) ((rad) * RAD_TO_DEG)
#define sq(x) ((x) * (x))

unsigned long micros();
unsigned long millis();
void delay(unsigned long ms);
void delayMicroseconds(unsigned int us);
void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t val);
int digitalRead(uint8_t pin);

// Restarts the virtual clock; the harnesses call this once per input.
void mock_reset_clock();

inline bool isDigit(int c) { return c >= '0' && c <= '9'; }

class Print {
public:
  virtual ~Print() {}
  virtual size_t write(uint8_t) { return 1; }
  size_t print(const char *) { return 1; }
  size_t print(const __FlashStringHelper *) { return 1; }
  size_t print(char) { return 1; }
  size_t print(int) { return 1; }
  size_t print(float, int = 2) { return 1; }
  size_t println(const char *) { return 1; }
  size_t println(const __FlashStringHelper *) { return 1; }
  size_t println(char) { return 1; }
  size_t println(int) { return 1; }
  size_t println(float, int = 2) { return 1; }
  size_t println() { return 1; }
};

class Stream : public Print {
public:
  virtual int available() = 0;
  virtual int read() = 0;
  virtual int peek() = 0;
};

class StringSumHelper {
public:
  StringSumHelper(const char *s = "") : s_(s) {}
  const char *c_str() const { return s_; }
private:
  const char *s_;
};

extern Print Serial;

#endif
