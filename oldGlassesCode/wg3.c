/* wg2.c
 * this program transmits dmx-style rgb packets
 * blink white forever
 * broadcasts from PC/Xbee to Xbee or Mrf/Arduino
 * 
 * Benjamin Jeffery
 * University of Idaho
 * 09/08/2015
 * sources: portions from uidaho/pulse_async_test.cpp,
 * Christopher Baker/Jacques Fortier's COBS Consistent Overhead Byte Stuffing
 * millisec() is copied from unicon/src/common/time.c 
 * for testing purposes. Thanks Clint
 */

#include <ftdi.h>
#include <stdio.h>
#include <time.h>
#include <sys/times.h>
#include <signal.h>

#define dot 50000
#define dash 150000

static size_t encode(const uint8_t* source, size_t size, uint8_t* destination);
static size_t getEncodedBufferSize(size_t sourceSize);
long millisec();
uint8_t sorc[96] = { };
uint8_t dest[96] = { };
static int exitRequested = 0;
// sigintHandler so we can exit gracefully when the user hits ctrl-C.
static void sigintHandler(int signum)
{
  exitRequested = 1;
}

int main()
{
  struct ftdi_context *ftdi;
  struct ftdi_device_list *devlist, *curdev;
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

  uint8_t whitePack[96] = {255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255};
  uint8_t redPack[96] = {255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0};
  uint8_t greenPack[96] = {0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0};
  uint8_t bluePack[96] = {0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255};
  uint8_t rgPack[96] = {255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0, 0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0};

  uint8_t gbPack[96] =
{0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0, 0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255};

  uint8_t brPack[96] = {0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255, 255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0,255,0,0};


  size_t n = sizeof(whitePack);
  size_t l = sizeof(dest);
  size_t m = getEncodedBufferSize(l);

  printf("Hello, welcome to Ben's Broadcast Test!\n");

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

  signal(SIGINT, sigintHandler);

  while (!exitRequested)
    {
 	// on for 1 sec
	nbytes = ftdi_write_data(ftdi, whitePack, m);
	usleep(475000);

	// off for 1 sec
	nbytes = ftdi_write_data(ftdi, dPack, m);
	usleep(475000);

 	// on for 1 sec
	nbytes = ftdi_write_data(ftdi, rgPack, m);
	usleep(475000);

	// off for 1 sec
	nbytes = ftdi_write_data(ftdi, dPack, m);
	usleep(475000);

 	// on for 1 sec
	nbytes = ftdi_write_data(ftdi, gbPack, m);
	usleep(475000);

	// off for 1 sec
	nbytes = ftdi_write_data(ftdi, dPack, m);
	usleep(475000);

 	// on for 1 sec
	nbytes = ftdi_write_data(ftdi, brPack, m);
	usleep(475000);

	// off for 1 sec
	nbytes = ftdi_write_data(ftdi, dPack, m);
	usleep(475000);
    }

  signal(SIGINT, SIG_DFL);

    
  if ((ret = ftdi_usb_close(ftdi)) < 0)
    {
      fprintf(stderr, "unable to close ftdi1: %d (%s)\n", ret, ftdi_get_error_string(ftdi));
      return 1;
    }

  ftdi_free(ftdi);
  
  printf("\nEnd of program.\n");

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


