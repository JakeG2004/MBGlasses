#include <ftdi.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <curses.h>
#include <string.h>
#include <ctype.h>

#define DAB   50000

static size_t getEncodedBufferSize(size_t sourceSize) {
    return sourceSize;
}

static void print_response(const char *label, unsigned char *buf, int len);
static int read_with_timeout(struct ftdi_context *ftdi, unsigned char *buf, int bufsize, int timeout_ms);
static void interactive_at_mode(struct ftdi_context *ftdi);

int main()
{
    struct ftdi_context *ftdi;
    struct ftdi_device_list *devlist, *curdev;
    char manufacturer[128], description[128];
    int retval = EXIT_SUCCESS;
    char letter;
    int i = 0;
    int ret = 0;
    int res;
    int nbytes;
    int f;
        
    if ((ftdi = ftdi_new()) == 0)
    {
        fprintf(stderr, "ftdi_new failed\n");
        return EXIT_FAILURE;
    } else {
        fprintf(stderr, "ftdi_new success\n");
    }
    
    if ((res = ftdi_usb_find_all(ftdi, &devlist, 0x0403, 0x6001)) <= 0) {
        fprintf(stderr, "no ftdi devices found\n");
        ftdi_list_free(&devlist);
        ftdi_free(ftdi);
        return 1;
    }
    
    if ((ret = ftdi_usb_open_dev(ftdi, devlist->dev)) < 0)
    {
        fprintf(stderr, "unable to open ftdi: %d (%s)\n", ret, ftdi_get_error_string(ftdi));
        ftdi_free(ftdi);
        return ret;            
    }
    
    ftdi_set_baudrate(ftdi, 57600);
    ftdi_set_line_property(ftdi, BITS_8, STOP_BIT_1, NONE);
    
    ftdi_list_free(&devlist);
    printf("Broadcasting initialized.\n");

    interactive_at_mode(ftdi);
    
    ftdi_disable_bitbang(ftdi);
    ftdi_usb_close(ftdi);
    ftdi_free(ftdi);
    
    return retval;
    }

static int read_with_timeout(struct ftdi_context *ftdi, unsigned char *buf, int bufsize, int timeout_ms) {
    int total = 0;
    int elapsed = 0;
    const int poll_us = 20000;

    while (elapsed < timeout_ms * 1000 && total < bufsize - 1) {
        int n = ftdi_read_data(ftdi, buf + total, bufsize - 1 - total);
        if (n > 0) {
            total += n;
            usleep(poll_us);
            elapsed += poll_us;
        } else if (n < 0) {
            break;
        } else {
            usleep(poll_us);
            elapsed += poll_us;
        }
    }
    return total;
}

static void print_response(const char *label, unsigned char *buf, int len) {
    if (len > 0) {
        buf[len] = '\0';
        while (len > 0 && (buf[len-1] == '\r' || buf[len-1] == '\n')) {
            buf[--len] = '\0';
        }
        printf("%s: %s\n", label, buf);
    } else {
        printf("%s: (no response)\n", label);
    }
    fflush(stdout);
}

static void interactive_at_mode(struct ftdi_context *ftdi) {
    unsigned char read_buf[256];
    int read_len;
    char line[128];

    while (ftdi_read_data(ftdi, read_buf, sizeof(read_buf)) > 0);

    printf("Entering XBee AT command mode...\n");
    usleep(100);

    ftdi_write_data(ftdi, (const unsigned char *)"+++", 3);
    usleep(1100000);

    read_len = read_with_timeout(ftdi, read_buf, sizeof(read_buf), 500);
    print_response("Enter AT mode", read_buf, read_len);

    printf("Now in interactive AT mode. Type AT commands (e.g. ATCH, ATID).\n");
    printf("Enter 'reconnect' to reconnect if commands stop working.\n");
    printf("Enter blank line or 'exit' to leave command mode.\n\n");

    while (1) {
        printf("AT> ");
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            break;
        }

        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = '\0';
        }

        if(strcasecmp(line, "reconnect") == 0) {
            interactive_at_mode(ftdi);
        }

        if (len == 0 || strcasecmp(line, "exit") == 0) {
            break;
        }

        char cmd[130];
        snprintf(cmd, sizeof(cmd), "%s\r", line);

        ftdi_write_data(ftdi, (const unsigned char *)cmd, strlen(cmd));
        usleep(100);

        read_len = read_with_timeout(ftdi, read_buf, sizeof(read_buf), 500);
        print_response("Response", read_buf, read_len);
    }

    const char *exit_cmd = "ATCN\r";
    ftdi_write_data(ftdi, (const unsigned char *)exit_cmd, strlen(exit_cmd));
    usleep(100000);

    read_len = read_with_timeout(ftdi, read_buf, sizeof(read_buf), 300);
    print_response("Exit AT mode", read_buf, read_len);

    printf("Exited command mode.\n");
}
