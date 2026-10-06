
#include <SoftPWM_timer.h>
#include <SoftPWM.h>

#include <SPI.h>

/*
* uiGC0 
* Goofy Controller 0
* common cathode LEDs +
* Mrf24j40
* 
* Derek McNee | Team Prism
* University of Idaho
* 9/27/2026
* 
*/

int redPin = 3;
int greenPin = 4;
int bluePin = 5;

const COLORS = [
  224, 228, 232, 240, 244, 248, 252,
  220, 188, 156, 124, 92, 60, 28,
  29, 30, 31,
  27, 23, 19, 15, 11, 7, 3,
  35, 67, 99, 131, 163, 195, 227,
  226, 225
];

uint8_t snake332(uint8_t) {
  uint8_t r_i = n >> 5;
  uint8_t g_i = (n >> 2) & 7;
  uint8_t b_i = n & 3;

  uint8_t g = (r_i & 1) ? 7 - g_i : g_i;
  uint8_t b = (g_i & 1) ? 3 - b_i : b_i;

  return (r_i << 5) | (g << 2) | b;
}

void setup() {
  SoftPWMBegin();
  SoftPWMSet(redPin, 0);
  SoftPWMSet(greenPin, 0);
  pinMode(bluePin, OUTPUT);
  SoftPWMSetFadeTime(redPin, 10, 10);
  SoftPWMSetFadeTime(greenPin, 10, 10);

  setColor(0,0,0);
}

void loop()
{
  for(uint8_t i = 0; i < 256; i++)
  {
    setColor(snake332(i));
    delay(100);
  }
}

void setColor(uint8_t color) // Example Gold = 248 
{
  int red = ((color >> 5) & 0b00000111) * 36;
  int green = ((color >> 2) & 0b00000111) * 36;
  int blue = (color & 0b00000011) * 85;
  SoftPWMSet(redPin, red);
  SoftPWMSet(greenPin, green);
  analogWrite(bluePin, blue);
}
