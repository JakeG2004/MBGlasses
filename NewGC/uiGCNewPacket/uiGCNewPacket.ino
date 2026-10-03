
#include <SoftPWM_timer.h>
#include <SoftPWM.h>

#include "mrf24j.h"
#include <SPI.h>

/*
* uiGC0 
* Goofy Controller 0
* common cathode LEDs +
* Mrf24j40
* 
* Benjamin Jeffery
* University of Idaho
* 10/09/2015
* 
*/

int redPin = 3;
int greenPin = 4;
int bluePin = 5;
int pin_reset = 6;
int pin_cs = 8;
int pin_interrupt = 2;
int idVal;
int interrupt_flag = 0;

Mrf24j mrf(pin_reset, pin_cs, pin_interrupt);

void setup() {
  DDRC = 0x00;
  SoftPWMBegin();
  SoftPWMSet(redPin, 0);
  SoftPWMSet(greenPin, 0);
  pinMode(bluePin, OUTPUT);
  SoftPWMSetFadeTime(redPin, 10, 10);
  SoftPWMSetFadeTime(greenPin, 10, 10);
  
  digitalWrite(A0,HIGH);
  digitalWrite(A1, HIGH);
  digitalWrite(A2, HIGH);
  digitalWrite(A3, HIGH);

  setColor(0,0,0);
  
  mrf.reset();
  mrf.init();
  mrf.set_pan(2015);
  mrf.set_channel(0x0C);
  mrf.address16_write(0x4202);
  mrf.set_promiscuous(true);
  mrf.set_bufferPHY(true);
  
  attachInterrupt(0, interrupt_routine, CHANGE);
  interrupts();
  idVal = ~PINC & 0x0f;
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
