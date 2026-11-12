#include "Arduino.h"

// Every micros() call advances the clock by 100us, delay() by its duration: time moves forward
// deterministically, so the library's dt computations behave like a running loop.
static unsigned long g_clock_us = 0;

void mock_reset_clock() { g_clock_us = 0; }
unsigned long micros() { g_clock_us += 100; return g_clock_us; }
unsigned long millis() { return g_clock_us / 1000; }
void delay(unsigned long ms) { g_clock_us += ms * 1000UL; }
void delayMicroseconds(unsigned int us) { g_clock_us += us; }
void pinMode(uint8_t, uint8_t) {}
void digitalWrite(uint8_t, uint8_t) {}
int digitalRead(uint8_t) { return LOW; }

Print Serial;
