#pragma once

#include <cstdint>
#include <cstring>

using byte = uint8_t;
using word = uint16_t;
using boolean = bool;

constexpr int LOW = 0, HIGH = 1, INPUT = 0, OUTPUT = 1, CHANGE = 1;
constexpr int A0 = 14, A1 = 15, A2 = 16, A3 = 17;
extern uint8_t DDRC, PINC, SREG;

void pinMode(int pin, int mode);
void digitalWrite(int pin, int value);
void analogWrite(int pin, int value);
void delay(unsigned long milliseconds);
void attachInterrupt(int interrupt, void (*handler)(), int mode);
void noInterrupts();
void interrupts();