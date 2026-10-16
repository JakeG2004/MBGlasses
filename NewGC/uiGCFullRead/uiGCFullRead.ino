
#include <SoftPWM_timer.h>
#include <SoftPWM.h>

#include "mrf24j.h"
#include <SPI.h>


// Pin mappings for DS (DipSwitches)
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
* Derek McNee
* University of Idaho
* 10/03/2026
* 
*/

int redPin = 3;
int greenPin = 4;
int bluePin = 5;
int pin_reset = 6;
int pin_cs = 8;
int pin_interrupt = 2;
uint8_t idVal = 0;
volatile uint8_t interrupt_flag = 0;

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

Mrf24j mrf(pin_reset, pin_cs, pin_interrupt);

void setup() {
  idVal = readID();

  // The read for the ID interferes with radio stuff
  // This fixes it after the ID is read
  pinMode(10, OUTPUT);
  digitalWrite(10, HIGH);
  SPCR |= _BV(MSTR); 

  SoftPWMBegin();
  SoftPWMSet(redPin, 0);
  SoftPWMSet(greenPin, 0);
  pinMode(bluePin, OUTPUT);
  SoftPWMSetFadeTime(redPin, 10, 10);
  SoftPWMSetFadeTime(greenPin, 10, 10);

  setColor(0);
  
  mrf.reset();
  mrf.init();
  mrf.set_pan(2015);
  mrf.set_channel(0x0C);
  mrf.address16_write(0x4202);
  mrf.set_promiscuous(true);
  mrf.set_bufferPHY(true);
  
  attachInterrupt(0, interrupt_routine, CHANGE);
  interrupts();
}

void interrupt_routine()
{
  interrupt_flag = 1;
}

void loop()
{
  if(interrupt_flag) {
    mrf.interrupt_handler();
    interrupt_flag = 0;
  }
  mrf.check_flags(&handle_rx, &handle_tx);
}

void handle_rx()
{
  setColor(mrf.get_rxinfo()->rx_data[idVal]);
  mrf.rx_flush();
}

void handle_tx()
{
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
