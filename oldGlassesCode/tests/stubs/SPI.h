#pragma once

#include "Arduino.h"

constexpr int MSBFIRST = 1, SPI_MODE0 = 0;
struct SPIClass {
    void setBitOrder(int) {}
    void setDataMode(int) {}
    void begin() {}
    byte transfer(byte value);
};
extern SPIClass SPI;