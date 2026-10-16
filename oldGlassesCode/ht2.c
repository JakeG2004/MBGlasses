/* ht2.c
 * this program transmits dmx-style rgb packets
 * 
 * broadcasts from PC/Xbee to Mrf/Arduino
 *
 * 
 * Benjamin Jeffery
 * University of Idaho
 * 10/14/2015
 * millisec() is copied from unicon/src/common/time.c 
 * for testing purposes. Thanks Clint
 */

#include <ftdi.h>
#include <stdio.h>
#include <time.h>
#include <sys/times.h>
#include <curses.h>
//#include <signal.h>

#define DOT 100000
#define DASH 300000
#define DORK 200000
#define DAB 40000
#define SLP 10000

static size_t encode(const uint8_t* source, size_t size, uint8_t* destination);
static size_t getEncodedBufferSize(size_t sourceSize);
long millisec();
uint8_t sorc[96] = { };
uint8_t dest[96] = { };


int main()
{
  struct ftdi_context *ftdi;
  struct ftdi_device_list *devlist, *curdev;
  char letter;
  char metter;
  bool news;
  int i = 0;
  int j = 0;
  int k = 0;
  int ret = 0;
  int res;
  int xbee_stat;
  int nbytes;
  int pm;
  int f;
  size_t write_index;
  long strt, stp;

  uint8_t dPack[96] = { };
  uint8_t whitePack[96] = {255,255,215, 255,255,215, 255,255,215, 255,255,215, 255,255,215, 255,255,215, 255,255,215, 255,255,215, 255,255,215, 255,255,215, 255,255,215, 255,255,215, 255,255,215, 255,255,215, 255,255,215, 255,255,215, 255,255,215, 255,255,215, 255,255,215, 255,255,215, 255,255,215, 255,255,215, 255,255,215, 255,255,215, 255,255,215, 255,255,215, 255,255,215, 255,255,215, 255,255,215, 255,255,215, 255,255,215, 255,255,215};
  uint8_t whit2Pack[96] = {255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255};

  uint8_t redPack[96] = {255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0};
  uint8_t greenPack[96] = {0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0};
  uint8_t bluePack[96] = {0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255};
  uint8_t yellowPack[96] = {255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0};
  uint8_t cyanPack[96] = {0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255};
  uint8_t magentaPack[96] = {255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255,255,0,255};
  uint8_t indegoPack[96] = {75,0,130,75,0,130,75,0,130,75,0,130,75,0,130,75,0,130,75,0,130,75,0,130,75,0,130,75,0,130,75,0,130,75,0,130,75,0,130,75,0,130,75,0,130,75,0,130,75,0,130,75,0,130,75,0,130,75,0,130,75,0,130,75,0,130,75,0,130,75,0,130,75,0,130,75,0,130,75,0,130,75,0,130,75,0,130,75,0,130,75,0,130,75,0,130};
  uint8_t coralPack[96] = {255,127,80,255,127,80,255,127,80,255,127,80,255,127,80,255,127,80,255,127,80,255,127,80,255,127,80,255,127,80,255,127,80,255,127,80,255,127,80,255,127,80,255,127,80,255,127,80,255,127,80,255,127,80,255,127,80,255,127,80,255,127,80,255,127,80,255,127,80,255,127,80,255,127,80,255,127,80,255,127,80,255,127,80,255,127,80,255,127,80,255,127,80,255,127,80};
  uint8_t dodgerPack[96] = {30,144,255,30,144,255,30,144,255,30,144,255,30,144,255,30,144,255,30,144,255,30,144,255,30,144,255,30,144,255,30,144,255,30,144,255,30,144,255,30,144,255,30,144,255,30,144,255,30,144,255,30,144,255,30,144,255,30,144,255,30,144,255,30,144,255,30,144,255,30,144,255,30,144,255,30,144,255,30,144,255,30,144,255,30,144,255,30,144,255,30,144,255,30,144,255};
  uint8_t goldPack[96] = {255,215,0,255,215,0,255,215,0,255,215,0,255,215,0,255,215,0,255,215,0,255,215,0,255,215,0,255,215,0,255,215,0,255,215,0,255,215,0,255,215,0,255,215,0,255,215,0,255,215,0,255,215,0,255,215,0,255,215,0,255,215,0,255,215,0,255,215,0,255,215,0,255,215,0,255,215,0,255,215,0,255,215,0,255,215,0,255,215,0,255,215,0,255,215,0};
  uint8_t ui1Pack[96] = {255,215,0, 255,255,215, 255,215,0, 255,255,215, 255,215,0, 255,255,215, 255,215,0, 255,255,215, 255,215,0, 255,255,215, 255,215,0, 255,255,215, 255,215,0, 255,255,215, 255,215,0, 255,255,215, 255,215,0, 255,255,215, 255,215,0, 255,255,215, 255,215,0, 255,255,215, 255,215,0, 255,255,215, 255,215,0, 255,255,215, 255,215,0, 255,255,215, 255,215,0, 255,255,215, 255,215,0, 255,255,215};
  uint8_t ui2Pack[96] = {255,255,215, 255,215,0, 255,255,215, 255,215,0, 255,255,215, 255,215,0, 255,255,215, 255,215,0, 255,255,215, 255,215,0, 255,255,215, 255,215,0, 255,255,215, 255,215,0, 255,255,215, 255,215,0, 255,255,215, 255,215,0, 255,255,215, 255,215,0, 255,255,215, 255,215,0, 255,255,215, 255,215,0, 255,255,215, 255,215,0, 255,255,215, 255,215,0, 255,255,215, 255,215,0, 255,255,215, 255,215,0};

  uint8_t rain1Pack[96] = {255,255,255, 255,0,0, 0,255,0, 0,0,255, 255,255,0, 0,255,255, 255,0,255, 0,0,0, 255,127,80, 30,144,255, 255,215,0, 220,20,60, 141,239,79, 0,0,0, 0,206,209, 186,85,211, 255,255,255, 255,0,0, 0,255,0, 0,0,255, 255,255,0, 0,255,255, 255,0,255, 0,0,0, 255,127,80, 30,144,255, 255,215,0, 220,20,60, 141,239,79, 0,0,0, 0,206,209, 186,85,211};
  uint8_t rain2Pack[96] = {186,85,211, 255,255,255, 255,0,0, 0,255,0, 0,0,255, 255,255,0, 0,255,255, 255,0,255, 75,0,130, 255,127,80, 0,0,0, 255,215,0, 0,0,0, 141,239,79, 0,0,0, 0,206,209, 186,85,211, 255,255,255, 255,0,0, 0,255,0, 0,0,255, 255,255,0, 0,255,255, 255,0,255, 0,0,0, 255,127,80, 30,144,255, 255,215,0, 0,0,0, 141,239,79, 131,194,209, 0,206,209};
  uint8_t rain3Pack[96] = {0,206,209, 186,85,211, 255,255,255, 255,0,0, 0,255,0, 0,0,255, 255,255,0, 0,255,255, 255,0,255, 0,0,0, 255,127,80, 30,144,255, 255,215,0, 220,20,60, 141,239,79, 0,0,0, 0,206,209, 186,85,211, 255,255,255, 255,0,0, 0,255,0, 0,0,255, 255,255,0, 0,255,255, 255,0,255, 0,0,0, 255,127,80, 30,144,255, 255,215,0, 220,20,60, 141,239,79, 0,0,0};
  uint8_t rain4Pack[96] = {0,0,0, 0,206,209, 186,85,211, 255,255,255, 255,0,0, 0,255,0, 0,0,255, 255,255,0, 0,255,255, 255,0,255, 0,0,0, 255,127,80, 30,144,255, 255,215,0, 220,20,60, 141,239,79, 31,194,109, 0,206,209, 186,85,211, 255,255,255, 255,0,0, 0,255,0, 0,0,255, 255,255,0, 0,255,255, 255,0,255, 0,0,0, 255,127,80, 30,144,255, 255,215,0, 220,20,60, 141,239,79};
  uint8_t rain5Pack[96] = {141,239,79, 31,194,109, 0,206,209, 186,85,211, 255,255,255, 255,0,0, 0,255,0, 0,0,255, 255,255,0, 0,255,255, 255,0,255, 0,0,0, 255,127,80, 30,0,55, 255,215,0, 220,20,60, 141,239,79, 31,194,109, 0,206,209, 186,85,211, 255,255,255, 255,0,0, 0,255,0, 0,0,255, 255,255,0, 0,255,255, 255,0,255, 75,0,0, 255,127,80, 0,0,0, 255,215,0, 220,20,60};
  uint8_t rain6Pack[96] = {220,20,60, 141,239,79, 0,0,0, 0,206,209, 186,85,211, 255,255,255, 255,0,0, 0,255,0, 0,0,255, 255,255,0, 0,255,255, 255,0,255, 0,0,0, 255,127,80, 30,144,255, 255,215,0, 220,20,60, 141,239,79, 31,0,0, 0,206,209, 186,85,211, 255,255,255, 255,0,0, 0,255,0, 0,0,255, 255,255,0, 0,255,255, 255,0,255, 0,0,0, 255,127,80, 30,144,255, 255,215,0};
  uint8_t rain7Pack[96] = {255,215,0, 220,20,60, 141,239,79, 31,194,109, 0,206,209, 186,85,211, 255,255,255, 255,0,0, 0,255,0, 0,0,255, 255,255,0, 0,255,255, 255,0,255, 75,0,130, 255,127,80, 30,144,255, 255,215,0, 220,20,60, 141,239,79, 31,194,109, 0,206,209, 186,85,211, 255,255,255, 255,0,0, 0,255,0, 0,0,255, 255,255,0, 0,255,255, 255,0,255, 75,0,130, 255,127,80, 30,144,255};
  uint8_t rain8Pack[96] = {30,144,255, 255,215,0, 220,20,60, 141,239,79, 31,194,109, 0,206,209, 186,85,211, 255,255,255, 255,0,0, 0,255,0, 0,0,255, 255,255,0, 0,255,255, 255,0,255, 75,0,130, 255,127,80, 30,144,255, 255,215,0, 220,20,60, 141,239,79, 31,194,109, 0,206,209, 186,85,211, 255,255,255, 255,0,0, 0,255,0, 0,0,255, 255,255,0, 0,255,255, 255,0,255, 75,0,130, 255,127,80};
  uint8_t rain9Pack[96] = {255,127,80, 30,144,255, 255,215,0, 220,20,60, 141,239,79, 31,194,109, 0,206,209, 186,85,211, 255,255,255, 255,0,0, 0,255,0, 0,0,255, 255,255,0, 0,255,255, 255,0,255, 75,0,130, 255,127,80, 30,144,255, 255,215,0, 220,20,60, 141,239,79, 31,194,109, 0,206,209, 186,85,211, 255,255,255, 255,0,0, 0,255,0, 0,0,255, 255,255,0, 0,255,255, 255,0,255, 75,0,130};
  uint8_t rain10Pack[96] = {75,0,130, 255,127,80, 30,144,255, 255,215,0, 220,20,60, 141,239,79, 31,194,109, 0,206,209, 186,85,211, 255,255,255, 255,0,0, 0,255,0, 0,0,255, 255,255,0, 0,255,255, 255,0,255, 75,0,130, 255,127,80, 30,144,255, 255,215,0, 220,20,60, 141,239,79, 31,194,109, 0,206,209, 186,85,211, 255,255,255, 255,0,0, 0,255,0, 0,0,255, 255,255,0, 0,255,255, 255,0,255};
  uint8_t rain11Pack[96] = {255,0,255, 75,0,130, 255,127,80, 30,144,255, 255,215,0, 220,20,60, 141,239,79, 31,194,109, 0,206,209, 186,85,211, 255,255,255, 255,0,0, 0,255,0, 0,0,255, 255,255,0, 0,255,255, 255,0,255, 75,0,130, 255,127,80, 30,144,255, 255,215,0, 220,20,60, 141,239,79, 31,194,109, 0,206,209, 186,85,211, 255,255,255, 255,0,0, 0,255,0, 0,0,255, 255,255,0, 0,255,255};
  uint8_t rain12Pack[96] = {0,255,255, 255,0,255, 75,0,130, 255,127,80, 30,144,255, 255,215,0, 220,20,60, 141,239,79, 31,194,109, 0,206,209, 186,85,211, 255,255,255, 255,0,0, 0,255,0, 0,0,255, 255,255,0, 0,255,255, 255,0,255, 75,0,130, 255,127,80, 30,144,255, 255,215,0, 220,20,60, 141,239,79, 31,194,109, 0,206,209, 186,85,211, 255,255,255, 255,0,0, 0,255,0, 0,0,255, 255,255,0};
  uint8_t rain13Pack[96] = {255,255,0, 0,255,255, 255,0,255, 75,0,130, 255,127,80, 30,144,255, 255,215,0, 220,20,60, 141,239,79, 31,194,109, 0,206,209, 186,85,211, 255,255,255, 255,0,0, 0,255,0, 0,0,255, 255,255,0, 0,255,255, 255,0,255, 75,0,130, 255,127,80, 30,144,255, 255,215,0, 220,20,60, 141,239,79, 31,194,109, 0,206,209, 186,85,211, 255,255,255, 255,0,0, 0,255,0, 0,0,255};
  uint8_t rain14Pack[96] = {0,0,255, 255,255,0, 0,255,255, 255,0,255, 75,0,130, 255,127,80, 30,144,255, 255,215,0, 220,20,60, 141,239,79, 31,194,109, 0,206,209, 186,85,211, 255,255,255, 255,0,0, 0,255,0, 0,0,255, 255,255,0, 0,255,255, 255,0,255, 75,0,130, 255,127,80, 30,144,255, 255,215,0, 220,20,60, 141,239,79, 31,194,109, 0,206,209, 186,85,211, 255,255,255, 255,0,0, 0,255,0};
  uint8_t rain15Pack[96] = {0,255,0, 0,0,255, 255,255,0, 0,255,255, 255,0,255, 75,0,130, 255,127,80, 30,144,255, 255,215,0, 220,20,60, 141,239,79, 31,194,109, 0,206,209, 186,85,211, 255,255,255, 255,0,0, 0,255,0, 0,0,255, 255,255,0, 0,255,255, 255,0,255, 75,0,130, 255,127,80, 30,144,255, 255,215,0, 220,20,60, 141,239,79, 31,194,109, 0,206,209, 186,85,211, 255,255,255, 255,0,0};
  uint8_t rain16Pack[96] = {255,0,0, 0,255,0, 0,0,255, 255,255,0, 0,255,255, 255,0,255, 75,0,130, 255,127,80, 30,144,255, 255,215,0, 220,20,60, 141,239,79, 31,194,109, 0,206,209, 186,85,211, 255,255,255, 255,0,0, 0,255,0, 0,0,255, 255,255,0, 0,255,255, 255,0,255, 75,0,130, 255,127,80, 30,144,255, 255,215,0, 220,20,60, 141,239,79, 31,194,109, 0,206,209, 186,85,211, 255,255,255};

  uint8_t twnk1Pack[96] = {55,0,0, 0,0,0, 0,155,0, 0,0,0, 0,0,55, 0,0,0, 155,0,0, 0,0,0, 0,55,0, 0,0,0, 0,0,55, 0,0,0, 55,0,0, 0,0,0, 0,55,0, 0,0,0, 0,0,155, 0,0,0, 55,0,0, 0,0,0, 0,55,0, 0,0,0, 0,0,55, 0,0,0, 155,115,0, 0,0,0, 0,55,0, 0,0,0, 0,0,55, 0,0,0, 55,0,0, 0,0,0};
 uint8_t twnk2Pack[96] = {0,0,0, 155,0,0, 0,0,0, 0,55,0, 0,0,0, 0,0,55, 0,0,0, 55,0,0, 0,0,0, 0,155,0, 0,0,0, 0,0,55, 0,0,0, 55,0,0, 0,0,0, 0,55,0, 0,0,0, 0,0,155, 0,0,0, 55,0,0, 0,0,0, 0,55,0, 0,0,0, 0,0,55, 0,0,0, 155,0,0, 0,0,0, 0,55,0, 0,0,0, 0,0,55, 0,0,0, 155,115,0};
 uint8_t twnk3Pack[96] = {0,0,55, 0,0,0, 55,0,0, 0,0,0, 0,55,0, 0,0,0, 0,0,155, 0,0,0, 55,0,0, 0,0,0, 0,55,0, 0,0,0, 0,0,55, 0,0,0, 155,0,0, 0,0,0, 0,55,0, 0,0,0, 0,0,55, 0,0,0, 55,0,0, 0,0,0, 0,155,0, 0,0,0, 0,0,55, 0,0,0, 55,0,0, 0,0,0, 0,55,0, 0,0,0, 0,0,155, 0,0,0};
 uint8_t twnk4Pack[96] = {155,0,0, 0,0,55, 0,155,0, 255,215,0, 0,0,155, 0,55,0, 155,0,0, 0,0,55, 0,155,0, 55,0,0, 0,0,155, 0,55,0, 155,0,0, 0,0,55, 0,155,0, 55,0,0, 0,0,155, 0,55,0, 155,0,0, 0,0,55, 0,155,0, 55,0,0, 0,0,155, 0,55,0, 155,0,0, 0,0,55, 255,115,0, 55,0,0, 0,0,155, 0,55,0, 155,0,0, 0,0,55};
  uint8_t twnk5Pack[96] = {0,155,0, 255,0,0, 0,0,155, 0,255,0, 155,0,0, 0,0,255, 0,155,0, 255,215,0, 0,0,155, 0,255,0, 155,0,0, 0,0,255, 0,155,0, 255,0,0, 0,0,155, 0,255,0, 155,0,0, 0,0,255, 0,155,0, 255,215,0, 0,0,155, 0,255,0, 155,0,0, 0,0,255, 0,155,0, 255,0,0, 0,0,155, 0,255,0, 155,0,0, 0,0,255, 0,155,0, 255,0,0};
  uint8_t twnk6Pack[96] = {0,0,255, 0,155,0, 255,215,0, 0,0,155, 0,255,0, 155,0,0, 0,0,255, 0,155,0, 255,0,0, 0,0,155, 0,255,0, 155,0,0, 0,0,255, 0,155,0, 255,0,0, 0,0,155, 0,255,0, 155,0,0, 0,0,255, 0,155,0, 255,0,0, 0,0,155, 0,255,0, 155,0,0, 0,0,255, 0,155,0, 255,0,0, 0,0,155, 0,255,0, 155,0,0, 0,0,255, 0,155,0};
  // gold is 255,215,0
  uint8_t twnk7Pack[96] = {155,0,0, 0,0,255, 0,155,0, 255,215,0, 0,0,155, 0,255,0, 155,0,0, 0,0,255, 0,155,0, 255,215,0, 0,0,155, 0,255,0, 155,0,0, 0,0,255, 0,155,0, 255,215,0, 0,0,155, 0,255,0, 155,0,0, 0,0,255, 0,155,0, 255,215,0, 0,0,155, 0,255,0, 155,0,0, 0,0,255, 0,155,0, 255,215,0, 0,0,155, 0,255,0, 155,0,0, 0,0,255};
  uint8_t twnk8Pack[96] = {0,255,0, 255,215,0, 0,0,155, 0,155,0, 255,215,0, 0,0,255, 0,255,0, 255,215,0, 0,0,255, 0,255,0, 255,215,0, 0,0,255, 0,255,0, 255,215,0, 0,0,255, 0,255,0, 255,215,0, 0,0,255, 0,255,0, 255,0,0, 0,0,255, 0,255,0, 255,215,0, 0,0,155, 0,255,0, 155,0,0, 0,0,255, 0,155,0, 255,215,0, 0,0,155, 0,255,0, 155,0,0};
  uint8_t twnk9Pack[96] = {255,255,255, 0,255,0, 255,215,0, 0,0,255, 0,255,0, 255,215,0, 0,0,255, 0,255,0, 255,215,0, 0,0,255, 0,255,0, 255,215,0, 0,0,255, 0,255,0, 255,215,0, 0,0,255, 0,255,0, 255,215,0, 0,0,255, 0,255,0, 255,0,0, 0,0,255, 0,255,0, 255,215,0, 0,0,155, 0,255,0, 155,0,0, 0,0,255, 0,155,0, 255,215,0, 0,0,155, 0,255,0};
  uint8_t twnk10Pack[96] = {255,215,0, 255,255,255, 0,255,0, 255,215,0, 0,0,255, 0,255,0, 255,215,0, 0,0,255, 0,255,0, 255,215,0, 0,0,255, 0,255,0, 255,215,0, 0,0,255, 0,255,0, 255,215,0, 0,0,255, 0,255,0, 255,215,0, 0,0,255, 0,255,0, 255,0,0, 0,0,255, 0,255,0, 255,215,0, 0,0,155, 0,255,0, 155,0,0, 0,0,255, 0,155,0, 255,215,0, 0,0,155};
  uint8_t twnk11Pack[96] = {0,155,0, 255,215,0, 255,255,255, 0,255,0, 255,215,0, 0,0,255, 0,255,0, 255,215,0, 0,0,255, 0,255,0, 255,215,0, 0,0,255, 0,255,0, 255,215,0, 0,0,255, 0,255,0, 255,215,0, 0,0,255, 0,255,0, 255,215,0, 0,0,255, 0,255,0, 255,0,0, 0,0,255, 0,255,0, 255,215,0, 0,0,155, 0,255,0, 155,0,0, 0,0,255, 0,155,0, 255,215,0};
  // half as bright
  uint8_t twnk12Pack[96] = {65,60,0, 0,0,0, 150,135,0, 0,0,0, 60,60,55, 0,0,0, 155,140,0, 0,0,0, 70,55,0, 0,0,0, 70,70,55, 0,0,0, 55,40,0, 0,0,0, 70,55,0, 0,0,0, 170,170,155, 0,0,0, 155,140,0, 0,0,0, 70,55,0, 0,0,0, 60,60,55, 0,0,0, 155,115,0, 0,0,0, 70,55,0, 0,0,0, 70,70,55, 0,0,0, 55,50,0, 0,0,0};
 uint8_t twnk13Pack[96] = {0,0,0, 155,140,0, 0,0,0, 60,55,0, 0,0,0, 70,70,55, 0,0,0, 55,50,0, 0,0,0, 170,155,0, 0,0,0, 60,60,55, 0,0,0, 55,50,0, 0,0,0, 60,55,0, 0,0,0, 170,170,155, 0,0,0, 55,50,0, 0,0,0, 60,55,0, 0,0,0, 60,60,55, 0,0,0, 155,140,0, 0,0,0, 60,55,0, 0,0,0, 60,60,55, 0,0,0, 155,115,0};
 uint8_t twnk14Pack[96] = {60,60,55, 0,0,0, 55,50,0, 0,0,0, 60,55,0, 0,0,0, 160,160,155, 0,0,0, 55,50,0, 0,0,0, 60,55,0, 0,0,0, 60,60,55, 0,0,0, 155,130,0, 0,0,0, 60,55,0, 0,0,0, 60,60,55, 0,0,0, 55,50,0, 0,0,0, 160,155,0, 0,0,0, 60,60,55, 0,0,0, 55,50,0, 0,0,0, 60,55,0, 0,0,0, 160,160,155, 0,0,0};
 uint8_t twnk15Pack[96] = {155,150,0, 60,60,55, 160,155,0, 255,215,0, 160,160,155, 60,55,0, 155,150,0, 60,60,55, 160,155,0, 55,50,0, 160,160,155, 60,55,0, 155,150,0, 60,60,55, 160,155,0, 55,50,0, 160,160,155, 60,55,0, 155,150,0, 60,60,55, 160,155,0, 55,50,0, 160,160,155, 60,55,0, 155,150,0, 60,60,55, 255,215,0, 55,50,0, 160,160,155, 60,55,0, 155,150,0, 60,60,55};
 uint8_t glowPack[96] = {155,150,0, 60,60,0, 160,155,0, 155,90,0, 160,160,0, 60,55,0, 155,150,0, 60,60,0, 160,155,0, 55,50,0, 160,160,0, 60,55,0, 155,150,0, 60,60,0, 160,155,0, 55,50,0, 160,160,0, 60,55,0, 155,150,0, 60,60,0, 160,155,0, 55,50,0, 160,160,0, 60,55,0, 155,150,0, 60,60,0, 255,215,0, 55,50,0, 160,160,0, 60,55,0, 155,150,0, 60,60,0};
  uint8_t rnPack[96] = {255,0,0, 255,0,0, 255,0,0, 0,0,0, 0,255,0, 0,255,0, 0,255,0, 0,0,0, 0,0,255, 0,0,255, 0,0,255, 0,0,0, 255,255,0, 255,255,0, 255,255,0, 0,0,0, 0,255,255, 0,255,255, 0,255,255, 0,0,0, 255,0,255, 255,0,255, 255,0,255, 0,0,0, 255,255,215, 255,255,215, 255,255,215, 0,0,0, 255,215,0, 255,215,0, 255,215,0, 0,0,0};
  uint8_t igPack[96] = {255,255,215, 255,255,215, 255,255,215, 0,0,0, 255,215,0, 255,215,0, 255,215,0, 0,0,0, 255,255,215, 255,255,215, 255,255,215, 0,0,0, 255,215,0, 255,215,0, 255,215,0, 0,0,0, 255,255,215, 255,255,215, 255,255,215, 0,0,0, 255,215,0, 255,215,0, 255,215,0, 0,0,0, 255,255,215, 255,255,215, 255,255,215, 0,0,0, 255,215,0, 255,215,0, 255,215,0, 0,0,0};

  size_t n = sizeof(whitePack);
  size_t l = sizeof(dest);
  size_t m = getEncodedBufferSize(l);

  printf("Hello, welcome to Ben's Halftime Toolkit!\n");

  // init 
  if ((ftdi = ftdi_new()) == 0)
    {
      fprintf(stderr, "ftdi_new failed\n");
      return 1;
    } else {
    fprintf(stderr, "ftdi_new success\n");
  }
  if ((res = ftdi_usb_find_all(ftdi, &devlist, 0x0403, 0x6001)) <0) {
    fprintf(stderr, "no ftdi devices found\n");
    ftdi_list_free(&devlist);
    ftdi_free(ftdi);
    return 1;
  } else {
    fprintf(stderr, "%d ftdi devices found.\n", res);
  }

  // alocate and initialize ftdi context
  if ((ret = ftdi_usb_open_dev(ftdi, devlist[0].dev)) < 0)
    {
      fprintf(stderr, "unable to open ftdi: %d (%s)\n", ret, ftdi_get_error_string(ftdi));
      ftdi_free(ftdi);
      return ret;            
    }
  else {
    fprintf(stderr, "ftdi_open successful\n");
  }

  ret = ftdi_set_baudrate(ftdi, 57600);
  if (ret < 0) {
    fprintf(stderr, "unable to set baud rate: %d (%s).\n", ret, ftdi_get_error_string(ftdi));
  } else {
    printf("baudrate set.\n");
  }

  f = ftdi_set_line_property(ftdi, 8, STOP_BIT_1, NONE);
  if(f < 0) {
    fprintf(stderr, "unable to set line parameters: %d (%s).\n", ret, ftdi_get_error_string(ftdi));
  } else {
    printf("line parameters set.\n");
  }

  ftdi_list_free(&devlist);
  printf("broadcasting.\n");

  //open curses session
  initscr();
  raw();
  keypad(stdscr, TRUE);
  nodelay(stdscr, TRUE);
  noecho();
  attron(A_BOLD);
  printw("a=twinkle routine; r=red flash; e=green flash; b=blue flash\n");
  printw("d=dark; g=gold flash;  w=white flash\n");
  printw("n=gold 4D; o=white 4D\n");
  printw("m=magenta; y=yellow flash;  k=cyan flash\n");
  printw("i=idaho morse; o=go morse; \n");
  printw("; c=coral flash;  k=cyan flash\n");
  printw("f=twink8; h=twink9; j=twink10; l= twink11\n");
  printw("s=rainbow short;  p=rainbow med\n");
  printw("TYPING IN CAPS SENDS LETTERS IN MORSE CODE\n");
  printw("dot key <.> to quit\n");

  /* signal(SIGINT, sigintHandler); */


  do {
    letter = getch();
    switch(letter) {
    case 'A':    // A morse
      printw("%c", letter);
      refresh();
      // a DOT DASH
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(DASH);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      break;

    case 'b':  // blue flash
      nbytes = ftdi_write_data(ftdi, bluePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, bluePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, bluePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      break;
    case 'B':   //blue morse
      printw("%c", letter);
      refresh();
      // b DASH DOT DOT DOT
      nbytes = ftdi_write_data(ftdi, bluePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, bluePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, bluePack, m);
      usleep(DASH);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, bluePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, bluePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, bluePack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, bluePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, bluePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, bluePack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      break;

    case 'c':  // coral flash
      nbytes = ftdi_write_data(ftdi, coralPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, coralPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, coralPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      break;
    case 'C':    // coral morse
      printw("%c", letter);
      refresh();
      // DASH DOT DASH DOT
      nbytes = ftdi_write_data(ftdi, coralPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, coralPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, coralPack, m);
      usleep(DASH);
      
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      
      nbytes = ftdi_write_data(ftdi, coralPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, coralPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, coralPack, m);
      usleep(DOT);
      
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      
      nbytes = ftdi_write_data(ftdi, coralPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, coralPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, coralPack, m);
      usleep(DASH);
      
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      
      nbytes = ftdi_write_data(ftdi, coralPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, coralPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, coralPack, m);
      usleep(DOT);
      
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      break;
    case 'd':  // dark
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      break;
    case 'D':    //dodger blue morse
      printw("%c", letter);
      refresh();
      // d DASH DOT DOT
      nbytes = ftdi_write_data(ftdi, dodgerPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dodgerPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dodgerPack, m);
      usleep(DASH);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dodgerPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dodgerPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dodgerPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dodgerPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dodgerPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dodgerPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      break;

    case 'e':  // green flash
      nbytes = ftdi_write_data(ftdi, greenPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, greenPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, greenPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      break;
    case 'E':  // green morse
      // e dot
      printw("%c", letter);
      refresh();
      nbytes = ftdi_write_data(ftdi, greenPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, greenPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, greenPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      break;

    case 'f':   // twnk8 flash
      nbytes = ftdi_write_data(ftdi, twnk8Pack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, twnk8Pack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, twnk8Pack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      break;
    case 'F':    // morse
      printw("%c", letter);
      refresh();
      // f DOT DOT DASH DOT
      nbytes = ftdi_write_data(ftdi, redPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, redPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, redPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, redPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, redPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, redPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, redPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, redPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, redPack, m);
      usleep(DASH);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, redPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, redPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, redPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      break;

    case 'g':  // gold hold
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);

      break;
    case 'G':    // morse
      printw("%c", letter);
      refresh();
      // g DASH DASH DOT
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(DASH);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(DASH);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      break;

    case 'h':   // twnk9 flash
      nbytes = ftdi_write_data(ftdi, twnk9Pack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, twnk9Pack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, twnk9Pack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      break;
    case 'H':   //indego morse
      printw("%c", letter);
      refresh();
      // h DOT DOT DOT DOT
      nbytes = ftdi_write_data(ftdi, magentaPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, magentaPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, magentaPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, magentaPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, magentaPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, magentaPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, magentaPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, magentaPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, magentaPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, magentaPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, magentaPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, magentaPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      break;

    case 'I':  // morse
      printw("%c", letter);
      refresh();
      // i DOT DOT
      nbytes = ftdi_write_data(ftdi, indegoPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, indegoPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, indegoPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, indegoPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, indegoPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, indegoPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      break;

    case 'j':   // tnk10 flash
      nbytes = ftdi_write_data(ftdi, twnk10Pack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, twnk10Pack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, twnk10Pack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      break;
    case 'J':   // morse
      printw("%c", letter);
      refresh();
      // j DOT DASH DASH DASH
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(DASH);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(DASH);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(DASH);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      break;

    case 'k':  // cyan flash
      nbytes = ftdi_write_data(ftdi, cyanPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, cyanPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, cyanPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      break;
    case 'K':   // morse
      printw("%c", letter);
      refresh();
      // k DASH DOT DASH
      nbytes = ftdi_write_data(ftdi, yellowPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, yellowPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, yellowPack, m);
      usleep(DASH);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, yellowPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, yellowPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, yellowPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, yellowPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, yellowPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, yellowPack, m);
      usleep(DASH);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      break;

    case 'l':   // tnk11 flash
      nbytes = ftdi_write_data(ftdi, twnk11Pack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, twnk11Pack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, twnk11Pack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      break;
    case 'L':   // morse
      printw("%c", letter);
      refresh();
      // l DOT DASH DOT DOT
      nbytes = ftdi_write_data(ftdi, greenPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, greenPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, greenPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, greenPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, greenPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, greenPack, m);
      usleep(DASH);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, greenPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, greenPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, greenPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, greenPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, greenPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, greenPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      break;

    case 'm':  // magenta flash
      nbytes = ftdi_write_data(ftdi, magentaPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, magentaPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, magentaPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      break;
    case 'M':   // morse
      printw("%c", letter);
      refresh();
      // m DASH DASH
      nbytes = ftdi_write_data(ftdi, magentaPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, magentaPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, magentaPack, m);
      usleep(DASH);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, magentaPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, magentaPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, magentaPack, m);
      usleep(DASH);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      break;

    case 'n':  // gold hold
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(DASH);
      usleep(DASH);
      usleep(DASH);
      usleep(DASH);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      break;
    case 'N':   // morse
      printw("%c", letter);
      refresh();
      // n DASH DOT
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(DASH);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      break;

    case 'o':  // white hold
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      break;

    case 'O':   //dodger morse
      printw("%c", letter);
      refresh();
      // o DASH DASH DASH
      nbytes = ftdi_write_data(ftdi, dodgerPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dodgerPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dodgerPack, m);
      usleep(DASH);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dodgerPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dodgerPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dodgerPack, m);
      usleep(DASH);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dodgerPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dodgerPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dodgerPack, m);
      usleep(DASH);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DASH);
      break;

    case 'p':  // rainbow med
      nbytes = ftdi_write_data(ftdi, rain5Pack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, rain5Pack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, rain5Pack, m);
      usleep(DASH);
      usleep(DASH);
      usleep(DASH);
      usleep(DASH);
      nbytes = ftdi_write_data(ftdi, rain6Pack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, rain6Pack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, rain6Pack, m);
      usleep(DASH);
      usleep(DASH);
      usleep(DASH);
      usleep(DASH);
      nbytes = ftdi_write_data(ftdi, rain7Pack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, rain7Pack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, rain7Pack, m);
      usleep(DASH);
      usleep(DASH);
      usleep(DASH);
      usleep(DASH);
      nbytes = ftdi_write_data(ftdi, rain8Pack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, rain8Pack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, rain8Pack, m);
      usleep(DASH);
      usleep(DASH);
      usleep(DASH);
      usleep(DASH);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      break;
    case 'P':  // morse
      printw("%c", letter);
      refresh();
      // DOT DASH DASH DOT
      nbytes = ftdi_write_data(ftdi, coralPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, coralPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, coralPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, coralPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, coralPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, coralPack, m);
      usleep(DASH);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, coralPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, coralPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, coralPack, m);
      usleep(DASH);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, coralPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, coralPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, coralPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      break;

      // 'q' coded below
    case 'Q':  // morse
      printw("%c", letter);
      refresh();
      // q DASH DASH DOT DASH
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(DASH);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(DASH);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(DASH);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      break;

    case 'r':  // red flash
      nbytes = ftdi_write_data(ftdi, redPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, redPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, redPack, m);
      usleep(SLP);
      
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      break;
    case 'R':     // red morse
      printw("%c", letter);
      refresh();
      // r DOT DASH DOT
      nbytes = ftdi_write_data(ftdi, redPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, redPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, redPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, redPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, redPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, redPack, m);
      usleep(DASH);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, redPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, redPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, redPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      break;

    case 's':  // rainbow short
      nbytes = ftdi_write_data(ftdi, rain1Pack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, rain1Pack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, rain1Pack, m);
      usleep(DASH);
      usleep(DASH);
      nbytes = ftdi_write_data(ftdi, rain2Pack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, rain2Pack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, rain2Pack, m);
      usleep(DASH);
      usleep(DASH);
      nbytes = ftdi_write_data(ftdi, rain3Pack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, rain3Pack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, rain3Pack, m);
      usleep(DASH);
      usleep(DASH);
      nbytes = ftdi_write_data(ftdi, rain4Pack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, rain4Pack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, rain4Pack, m);
      usleep(DASH);
      usleep(DASH);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      break;
    case 'S':  // morse
      printw("%c", letter);
      refresh();
      // s DOT DOT DOT
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      break;

    case 'T':  // morse
      printw("%c", letter);
      refresh();
      // t DASH
      nbytes = ftdi_write_data(ftdi, greenPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, greenPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, greenPack, m);
      usleep(DASH);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      break;

    case 'U':  // morse
      printw("%c", letter);
      refresh();
      // u DOT DOT DASH
      nbytes = ftdi_write_data(ftdi, bluePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, bluePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, bluePack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, bluePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, bluePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, bluePack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, bluePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, bluePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, bluePack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DASH);
      break;

    case 'v':   // vandals morse
      // v DOT DOT DOT DASH
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(DASH);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      usleep(DASH);

      // a DOT DASH
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(DASH);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      usleep(DASH);

      // n DASH DOT
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(DASH);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      usleep(DASH);

      // d DASH DOT DOT
      nbytes = ftdi_write_data(ftdi, dodgerPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dodgerPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dodgerPack, m);
      usleep(DASH);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dodgerPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dodgerPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dodgerPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dodgerPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dodgerPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dodgerPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      usleep(DASH);

      // a DOT DASH
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(DASH);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      usleep(DASH);

      // l DOT DASH DOT DOT
      nbytes = ftdi_write_data(ftdi, greenPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, greenPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, greenPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, greenPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, greenPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, greenPack, m);
      usleep(DASH);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, greenPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, greenPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, greenPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, greenPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, greenPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, greenPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      usleep(DASH);

      // s DOT DOT DOT
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      usleep(DASH);

      break;
    case 'V':  // morse
      printw("%c", letter);
      refresh();
      // v DOT DOT DOT DASH
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(DASH);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      break;

    case 'w':  // white flash
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      break;
    case 'W':  // morse
      printw("%c", letter);
      refresh();
      // w DOT DASH DASH
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(DASH);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(DASH);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      break;

    case 'x':  // low gold hold
      nbytes = ftdi_write_data(ftdi, glowPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, glowPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, glowPack, m);
      usleep(SLP);
      break;

    case 'X':  // morse
      printw("%c", letter);
      refresh();
      // x DASH DOT DOT DASH
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(DASH);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(DASH);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      break;

    case 'y':  // yellow flash
      nbytes = ftdi_write_data(ftdi, yellowPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, yellowPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, yellowPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      break;
    case 'Y':  // morse
      printw("%c", letter);
      refresh();
      // y DASH DOT DASH DASH
      nbytes = ftdi_write_data(ftdi, yellowPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, yellowPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, yellowPack, m);
      usleep(DASH);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, yellowPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, yellowPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, yellowPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, yellowPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, yellowPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, yellowPack, m);
      usleep(DASH);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, yellowPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, yellowPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, yellowPack, m);
      usleep(DASH);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      break;

    case 'z':
      do {
	metter = getch();
	nbytes = ftdi_write_data(ftdi, twnk12Pack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, twnk12Pack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, twnk12Pack, m);
	usleep(DASH);
	usleep(DASH);

	nbytes = ftdi_write_data(ftdi, twnk13Pack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, twnk13Pack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, twnk13Pack, m);
	usleep(DASH);
	usleep(DASH);

	nbytes = ftdi_write_data(ftdi, twnk14Pack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, twnk14Pack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, twnk14Pack, m);
	usleep(DASH);
	usleep(DASH);

	nbytes = ftdi_write_data(ftdi, twnk15Pack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, twnk15Pack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, twnk15Pack, m);
	usleep(DASH);
	usleep(DASH);
      } while (metter != ',');      

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      break;
    case 'Z':  // morse
      printw("%c", letter);
      refresh();
      // z DASH DASH DOT DOT
      nbytes = ftdi_write_data(ftdi, indegoPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, indegoPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, indegoPack, m);
      usleep(DASH);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, indegoPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, indegoPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, indegoPack, m);
      usleep(DASH);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, indegoPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, indegoPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, indegoPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, indegoPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, indegoPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, indegoPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      break;

    case 'u':  // go morse
      printw("%c", letter);
      refresh();
      // g1 DASH DASH DOT
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(DASH);
      
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(DASH);
      
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(DOT);
      
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      usleep(DASH);

      // o1 DASH DASH DASH
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(DASH);
      
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(DASH);
      
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(DASH);
      
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DASH);
      usleep(DOT);

      //      g2 DASH DASH DOT
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(DASH);
      
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(DASH);
      
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(DOT);
      
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      usleep(DASH);

      // o2 DASH DASH DASH
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(DASH);
      
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(DASH);
      
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(DASH);
      
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DASH);
      usleep(DOT);

      // g3 DASH DASH DOT
      nbytes = ftdi_write_data(ftdi, ui1Pack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, ui1Pack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, ui1Pack, m);
      usleep(DASH);
      
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      
      nbytes = ftdi_write_data(ftdi, ui2Pack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, ui2Pack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, ui2Pack, m);
      usleep(DASH);
      
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      
      nbytes = ftdi_write_data(ftdi, ui1Pack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, ui1Pack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, ui1Pack, m);
      usleep(DOT);
      
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      usleep(DASH);

      //      o3 DASH DASH DASH
      nbytes = ftdi_write_data(ftdi, ui2Pack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, ui2Pack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, ui2Pack, m);
      usleep(DASH);
      
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      
      nbytes = ftdi_write_data(ftdi, ui1Pack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, ui1Pack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, ui1Pack, m);
      usleep(DASH);
      
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      
      nbytes = ftdi_write_data(ftdi, ui2Pack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, ui2Pack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, ui2Pack, m);
      usleep(DASH);
      
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DASH);
      break;

    case 'i':   // idaho morse
      printw("%c", letter);
      refresh();
     // i dot dot (white)
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(DOT);
      
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(DOT);
      
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      usleep(DASH);
      // d dash dot dot (gold)
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(DASH);
      
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(DOT);
      
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(DOT);
      
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      usleep(DASH);
      // a dot dash (white)
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(DOT);
      
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(DASH);
      
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      usleep(DASH);
      // h dot dot dot dot (gold)
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(DOT);
      
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(DOT);
      
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(DOT);
      
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);

      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, goldPack, m);
      usleep(DOT);
      
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      usleep(DASH);
      // o dash dash dash (white)
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(DASH);
      
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(DASH);
      
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DOT);
      
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, whitePack, m);
      usleep(DASH);
      
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(DASH);
      usleep(DOT);
      break;

    case 'a':  //asterion
      printw("%c", letter);
      refresh();
      // twink a little 4 beats
      for(i=0; i<4; i++) {
	nbytes = ftdi_write_data(ftdi, twnk1Pack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, twnk1Pack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, twnk1Pack, m);
	usleep(DASH);
	
	nbytes = ftdi_write_data(ftdi, twnk2Pack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, twnk2Pack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, twnk2Pack, m);
	usleep(DASH);

	nbytes = ftdi_write_data(ftdi, twnk3Pack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, twnk3Pack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, twnk3Pack, m);
	usleep(DASH);

	nbytes = ftdi_write_data(ftdi, twnk1Pack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, twnk1Pack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, twnk1Pack, m);
	usleep(DASH);
      }

      // twink more 4 beats
      for(i=0; i<4; i++) {
	nbytes = ftdi_write_data(ftdi, twnk4Pack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, twnk4Pack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, twnk4Pack, m);
	usleep(DORK);
	
	nbytes = ftdi_write_data(ftdi, twnk5Pack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, twnk5Pack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, twnk5Pack, m);
	usleep(DORK);

	nbytes = ftdi_write_data(ftdi, twnk6Pack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, twnk6Pack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, twnk6Pack, m);
	usleep(DORK);

	nbytes = ftdi_write_data(ftdi, twnk7Pack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, twnk7Pack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, twnk7Pack, m);
	usleep(DORK);
      }
      // twink emphatically 4 beats
      for(i=0; i<8; i++) {
	nbytes = ftdi_write_data(ftdi, twnk8Pack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, twnk8Pack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, twnk8Pack, m);
	usleep(DOT);
	
	nbytes = ftdi_write_data(ftdi, twnk9Pack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, twnk9Pack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, twnk9Pack, m);
	usleep(DOT);

	nbytes = ftdi_write_data(ftdi, twnk10Pack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, twnk10Pack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, twnk10Pack, m);
	usleep(DOT);

	nbytes = ftdi_write_data(ftdi, twnk11Pack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, twnk11Pack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, twnk11Pack, m);
	usleep(DOT);
      }
      usleep(DASH);
      usleep(DASH);

      //  gold flash
      for(i=0; i<4; i++)
	{
	  nbytes = ftdi_write_data(ftdi, goldPack, m);
	  usleep(SLP);
	  nbytes = ftdi_write_data(ftdi, goldPack, m);
	  usleep(SLP);
	  nbytes = ftdi_write_data(ftdi, goldPack, m);
	  usleep(SLP);

	  nbytes = ftdi_write_data(ftdi, dPack, m);
	  usleep(SLP);
	  nbytes = ftdi_write_data(ftdi, dPack, m);
	  usleep(SLP);
	  nbytes = ftdi_write_data(ftdi, dPack, m);
	  usleep(DASH);
	  usleep(DASH);
	}
      break;

    case 'q':   // rainbow cycle
      do {
	metter = getch();

	nbytes = ftdi_write_data(ftdi, rain1Pack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, rain1Pack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, rain1Pack, m);
	usleep(SLP);
	
	nbytes = ftdi_write_data(ftdi, dPack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, dPack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, dPack, m);
	
	nbytes = ftdi_write_data(ftdi, rain2Pack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, rain2Pack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, rain2Pack, m);
	usleep(SLP);
	
	nbytes = ftdi_write_data(ftdi, dPack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, dPack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, dPack, m);
	usleep(SLP);
	
	nbytes = ftdi_write_data(ftdi, rain3Pack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, rain3Pack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, rain3Pack, m);
	usleep(SLP);
	
	nbytes = ftdi_write_data(ftdi, dPack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, dPack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, dPack, m);
	usleep(SLP);
	
	nbytes = ftdi_write_data(ftdi, rain4Pack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, rain4Pack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, rain4Pack, m);
	usleep(SLP);

	nbytes = ftdi_write_data(ftdi, dPack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, dPack, m);
	usleep(SLP);
	nbytes = ftdi_write_data(ftdi, dPack, m);
	usleep(SLP);
	/* nbytes = ftdi_write_data(ftdi, rain5Pack, m); */
	/* usleep(SLP); */
	/* nbytes = ftdi_write_data(ftdi, rain5Pack, m); */
	/* usleep(SLP); */
	/* nbytes = ftdi_write_data(ftdi, rain5Pack, m); */
	/* usleep(SLP); */
	/* nbytes = ftdi_write_data(ftdi, rain6Pack, m); */
	/* usleep(SLP); */
	/* nbytes = ftdi_write_data(ftdi, rain6Pack, m); */
	/* usleep(SLP); */
	/* nbytes = ftdi_write_data(ftdi, rain6Pack, m); */
	/* usleep(SLP); */
	
	/* nbytes = ftdi_write_data(ftdi, rain7Pack, m); */
	/* usleep(SLP); */
	/* nbytes = ftdi_write_data(ftdi, rain7Pack, m); */
	/* usleep(SLP); */
	/* nbytes = ftdi_write_data(ftdi, rain7Pack, m); */
	/* usleep(DAB); */
	/* nbytes = ftdi_write_data(ftdi, rain8Pack, m); */
	/* usleep(SLP); */
	/* nbytes = ftdi_write_data(ftdi, rain8Pack, m); */
	/* usleep(SLP); */
	/* nbytes = ftdi_write_data(ftdi, rain8Pack, m); */
	/* usleep(DOT); */
	
	/* nbytes = ftdi_write_data(ftdi, rain9Pack, m); */
	/* usleep(SLP); */
	/* nbytes = ftdi_write_data(ftdi, rain9Pack, m); */
	/* usleep(SLP); */
	/* nbytes = ftdi_write_data(ftdi, rain9Pack, m); */
	/* usleep(DOT); */
	/* nbytes = ftdi_write_data(ftdi, rain10Pack, m); */
	/* usleep(SLP); */
	/* nbytes = ftdi_write_data(ftdi, rain10Pack, m); */
	/* usleep(SLP); */
	/* nbytes = ftdi_write_data(ftdi, rain10Pack, m); */
	/* usleep(DOT); */
	
	/* nbytes = ftdi_write_data(ftdi, rain11Pack, m); */
	/* usleep(SLP); */
	/* nbytes = ftdi_write_data(ftdi, rain11Pack, m); */
	/* usleep(SLP); */
	/* nbytes = ftdi_write_data(ftdi, rain11Pack, m); */
	/* usleep(DOT); */
	/* nbytes = ftdi_write_data(ftdi, rain12Pack, m); */
	/* usleep(SLP); */
	/* nbytes = ftdi_write_data(ftdi, rain12Pack, m); */
	/* usleep(SLP); */
	/* nbytes = ftdi_write_data(ftdi, rain12Pack, m); */
	/* usleep(DOT); */
	
	/* nbytes = ftdi_write_data(ftdi, rain13Pack, m); */
	/* usleep(SLP); */
	/* nbytes = ftdi_write_data(ftdi, rain13Pack, m); */
	/* usleep(SLP); */
	/* nbytes = ftdi_write_data(ftdi, rain13Pack, m); */
	/* usleep(DAB); */
	/* nbytes = ftdi_write_data(ftdi, rain14Pack, m); */
	/* usleep(SLP); */
	/* nbytes = ftdi_write_data(ftdi, rain14Pack, m); */
	/* usleep(SLP); */
	/* nbytes = ftdi_write_data(ftdi, rain14Pack, m); */
	/* usleep(DOT); */
	
	/* nbytes = ftdi_write_data(ftdi, rain15Pack, m); */
	/* usleep(SLP); */
	/* nbytes = ftdi_write_data(ftdi, rain15Pack, m); */
	/* usleep(SLP); */
	/* nbytes = ftdi_write_data(ftdi, rain15Pack, m); */
	/* usleep(DOT); */
	/* nbytes = ftdi_write_data(ftdi, rain16Pack, m); */
	/* usleep(SLP); */
	/* nbytes = ftdi_write_data(ftdi, rain16Pack, m); */
	/* usleep(SLP); */
	/* nbytes = ftdi_write_data(ftdi, rain16Pack, m); */
	/* usleep(DOT); */
      } while (metter != ',');      

      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      nbytes = ftdi_write_data(ftdi, dPack, m);
      usleep(SLP);
      break;
    default:
      // printw("%c", letter);
      usleep(DOT);
    }
    
  } while (letter != '.');
  /* signal(SIGINT, SIG_DFL); */
  
  endwin();   // close curses session
  
  if ((ret = ftdi_usb_close(ftdi)) < 0)
    {
      fprintf(stderr, "unable to close ftdi1: %d (%s)\n", ret, ftdi_get_error_string(ftdi));
      return 1;
    }

  ftdi_free(ftdi);
  
  printf("End of program.\n");
  
  return 0;
} // END main





  // COBS encoding removes "00" from the packet, allowing it to be used as
  // a packet dilimiter
  // \brief Encode a byte buffer with the COBS encoder.
  // \param source The buffer to encode.
  // \param size The size of the buffer to encode.
  // \param destination The target buffer for the encoded bytes.
  // \returns The number of bytes in the encoded buffer.
  // \warning destination must have a minimum capacity of
  //     (size + size / 254 + 1).
 static size_t encode(const uint8_t* source, size_t size, uint8_t* destination)
 {
  size_t read_index  = 0;
  size_t write_index = 1;
  size_t code_index  = 0;
  uint8_t code       = 1;

  while(read_index < size)
    {
      if(source[read_index] == 0)
	{
	  destination[code_index] = code;
	  code = 1;
	  code_index = write_index++;
	  read_index++;
	}
      else
	{
	  destination[write_index++] = source[read_index++];
	  code++;
	  
	  if(code == 0xFF)
	    {
	      destination[code_index] = code;
	      code = 1;
	      code_index = write_index++;
	    }
	}
    }
  
  destination[code_index] = code;
  
  return write_index;
} // END encode

static size_t getEncodedBufferSize(size_t sourceSize)
{
  size_t s;
  s = sourceSize + sourceSize / 254 + 1;
  //printf("buffer size is : %zd.\n", s);
  return s;
}



/*
 * Return elapsed CPU time.  This is CPU user time + system time.
 * this is borrowed from Clinton Jeffery/unicon
 */
long millisec()
{
  long usertime = 0;
  static long starttime = -2, clk_tck;
  long t;

#ifdef HAVE_GETRUSAGE
  struct rusage ruse;
  int i = getrusage(RUSAGE_SELF, &ruse);
  if (i == -1) return 0;
  return (ruse.ru_utime.tv_sec + ruse.ru_stime.tv_sec)*1000 +
    (ruse.ru_utime.tv_usec + ruse.ru_stime.tv_usec)/1000;
#else					/* HAVE_GETRUSAGE */

#ifdef HAVE_CLOCK_GETTIME
  { struct timespec ts;
    static long system_millisec;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    if (startime == -2)
      system_millisec = ts.tv_sec * 1000 + ts.tv_nsec/1000000;
    usertime = ts.tv_sec * 1000 + ts.tv_nsec/1000000 - system_millisec;
  }
#endif					/* HAVE_CLOCK_GETTIME */

/*
 * t's units here are (system-defined) clock ticks. If we have clock_gettime()
 * for user time, report system ticks, otherwise report user+system ticks.
 */
  {
    struct tms tp;
    times(&tp);
#ifdef HAVE_CLOCK_GETTIME
    t = (long) (tp.tms_stime);
#else					/* HAVE_CLOCK_GETTIME */
    t = (long) (tp.tms_utime + tp.tms_stime);
#endif					/* HAVE_CLOCK_GETTIME */
  }
  
  if (starttime == -2) {
    starttime = t;
#ifdef CLK_TCK
    clk_tck = CLK_TCK;
#else					/* CLK_TCK */
    clk_tck = sysconf(_SC_CLK_TCK);
#endif
  }

#ifdef HAVE_CLOCK_GETTIME
  return usertime + (long) ((1000.0 / clk_tck) * (t - starttime));
#else					/* HAVE_CLOCK_GETTIME */
  return (long) ((1000.0 / clk_tck) * (t - starttime));
#endif					/* HAVE_CLOCK_GETTIME */
#endif					/* HAVE_GETRUSAGE */
}


