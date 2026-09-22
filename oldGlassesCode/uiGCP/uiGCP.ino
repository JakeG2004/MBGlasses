#include <SoftPWM_timer.h>
#include <SoftPWM.h>

#include "mrf24j.h"
#include <SPI.h>

/*
* uiGCP
* Goofy Controller Prototype
* common anode LEDs +
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
int startId;
int idShif;

Mrf24j mrf(pin_reset, pin_cs, pin_interrupt);

void setup() {
  DDRC = 0x00;
  SoftPWMBegin();
  pinMode(redPin, OUTPUT);
  SoftPWMSet(greenPin, 0);
  pinMode(bluePin, OUTPUT);
  SoftPWMSetFadeTime(greenPin, 100, 100);

  digitalWrite(A0, HIGH);
  digitalWrite(A1, HIGH);
  digitalWrite(A2, HIGH);
  digitalWrite(A3, HIGH);
  

  Serial.begin(9600);
  setColor(255,255,255);
  
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
  idShif = idVal+16;
  startId = idShif * 3;
  
  Serial.println("hello, sw!");
  Serial.print("startId is ");
  Serial.println(startId, DEC);
  Serial.print("idVal is ");
  Serial.println(idVal, DEC);
  Serial.println(idShif, DEC);
}

void interrupt_routine()
{
  mrf.interrupt_handler();
}

void loop()
{
  if (idVal == 0x0f) {
    setColor(255, 255, 0);
    delay(350);
    setColor(255,255,255);
    delay(100);
    setColor(0, 255, 255);
    delay(350);
    setColor(255,255,255);
    delay(100);
    setColor(255, 0, 255);
    delay(350);
    setColor(255,255,255);
    delay(100);
    setColor(0, 65, 255);
    delay(350);
    setColor(255,255,255);
    delay(100);
  }
  else {
 mrf.check_flags(&handle_rx, &handle_tx); 
  }
}

void handle_rx()
{
  Serial.println(" ");

  Serial.print("startId is ");
  Serial.println(startId, DEC);

  Serial.print(mrf.get_rxinfo()->rx_data[startId], DEC);
  Serial.print(" ");
  Serial.print(mrf.get_rxinfo()->rx_data[startId+1], DEC);
  Serial.print(" ");
  Serial.println(mrf.get_rxinfo()->rx_data[startId+2], DEC);
  
  setColor(255-(mrf.get_rxinfo()->rx_data[startId]), 255-(mrf.get_rxinfo()->rx_data[startId+1]), 255-(mrf.get_rxinfo()->rx_data[startId+2]));
  mrf.rx_flush();
}

void handle_tx()
{
}

void setColor(int red, int green, int blue)
{
  analogWrite(redPin, red);
  SoftPWMSet(greenPin, green);
  analogWrite(bluePin, blue);
}


