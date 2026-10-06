
#include <SoftPWM_timer.h>
#include <SoftPWM.h>

#include <SPI.h>

#define DS1 14
#define DS2 15
#define DS3 16
#define DS4 17
#define DS5 18
#define DS6 19
#define DS7 9
#define DS8 10

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

uint8_t readID()
{
  const uint8_t pins[8] = {DS1, DS2, DS3, DS4, DS5, DS6, DS7, DS8};
  uint8_t id = 0;
  for (uint8_t i = 0; i < 8; i++)
  {
    pinMode(pins[i], INPUT_PULLUP);
    if (digitalRead(pins[i]) == LOW) 
      id |= (1 << i);
  }
  return id;
}

uint8_t COLORS[34] = {
  224, 228, 232, 240, 244, 248, 252,
  220, 188, 156, 124, 92, 60, 28,
  29, 30, 31,
  27, 23, 19, 15, 11, 7, 3,
  35, 67, 99, 131, 163, 195, 227,
  226, 225
};

uint8_t snake332(uint8_t n) {
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

  setColor(0);
}

void loop()
{
  if(readID() == 0) {
    for(uint8_t i = 0; i < 256; i++)
    {
      setColor(snake332(i));
      delay(50);
    }
  } else {
    for(int i = 0; i < 34; i++){
      setColor(COLORS[i]);
      delay(50);
    }
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
