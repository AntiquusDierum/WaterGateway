#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <time.h>

#define ERIC_PORT \
    "/dev/serial/by-id/usb-FTDI_FT231X_USB_UART_DM03PVN2-if00-port0"

#define BUFFER_SIZE 256

#define RTC_SYNC_TOLERANCE_SECONDS  10.0

typedef enum
{
    RTC_SYNC_IDLE = 0,
    RTC_SYNC_WAIT_COMMAND_MODE,
    RTC_SYNC_WAIT_SET_ACK,
    RTC_SYNC_WAIT_STREAM_MODE

} RtcSyncState_t;

static RtcSyncState_t rtc_sync_state = RTC_SYNC_IDLE;

static int configure_serial_port(int fd)
{
    struct termios tty;

    if (tcgetattr(fd, &tty) != 0)
    {
        perror("tcgetattr");
        return -1;
    }

    cfmakeraw(&tty);

    cfsetispeed(&tty, B38400);
    cfsetospeed(&tty, B38400);

    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~CRTSCTS;

    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &tty) != 0)
    {
        perror("tcsetattr");
        return -1;
    }

    return 0;
}
static int parse_stm_datetime(
    const char *message,
    time_t *stm_time)
{
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;

    struct tm tm_value = {0};

    if ((message == NULL) ||
        (stm_time == NULL))
    {
        return -1;
    }

    if (sscanf(
            message,
            "WB1,DATE=%d-%d-%d,TIME=%d:%d:%d",
            &year,
            &month,
            &day,
            &hour,
            &minute,
            &second) != 6)
    {
        return -1;
    }

    tm_value.tm_year = year - 1900;
    tm_value.tm_mon = month - 1;
    tm_value.tm_mday = day;
    tm_value.tm_hour = hour;
    tm_value.tm_min = minute;
    tm_value.tm_sec = second;

    /*
     * Let the C library determine whether daylight-saving
     * time applies to this local date/time.
     */
    tm_value.tm_isdst = -1;

    *stm_time = mktime(&tm_value);

    if (*stm_time == (time_t)-1)
    {
        return -1;
    }

    return 0;
}

static int serial_send_string(
    int fd,
    const char *text)
{
    size_t length;
    ssize_t result;

    if ((fd < 0) || (text == NULL))
    {
        return -1;
    }

    length = strlen(text);

    result = write(
        fd,
        text,
        length);

    if (result < 0)
    {
        perror("write");
        return -1;
    }

    if ((size_t)result != length)
    {
        fprintf(
            stderr,
            "Warning: incomplete serial write\n");

        return -1;
    }

    return 0;
}

static int send_setdt_command(int fd)
{
    time_t now;
    struct tm local_time;
    char command[64];

    now = time(NULL);

    if (localtime_r(&now, &local_time) == NULL)
    {
        fprintf(
            stderr,
            "Warning: could not obtain local time\n");

        return -1;
    }

    if (strftime(
            command,
            sizeof(command),
            "setdt %Y-%m-%d %H:%M:%S\r",
            &local_time) == 0U)
    {
        fprintf(
            stderr,
            "Warning: could not format setdt command\n");

        return -1;
    }

    return serial_send_string(
        fd,
        command);
}

static void process_message(int fd, const char *message)
{
    time_t stm_time;
    time_t pi_time;
    double difference;

    if (message == NULL)
    {
        return;
    }

    printf("%s\n", message);
    fflush(stdout);

    switch (rtc_sync_state)
    {
        case RTC_SYNC_IDLE:
        {
            if (strncmp(message, "WB1,", 4U) != 0)
            {
                break;
            }

            if (parse_stm_datetime(
                    message,
                    &stm_time) != 0)
            {
                fprintf(
                    stderr,
                    "Warning: could not parse STM32 date/time\n");

                break;
            }

            pi_time = time(NULL);

            difference = difftime(
                pi_time,
                stm_time);

            if (difference < 0.0)
            {
                difference = -difference;
            }

            if (difference > RTC_SYNC_TOLERANCE_SECONDS)
            {
                printf(
                    "STM32 RTC differs from Pi by %.1f seconds; "
                    "synchronising...\n",
                    difference);

                if (serial_send_string(
                        fd,
                        "\r") == 0)
                {
                    rtc_sync_state =
                        RTC_SYNC_WAIT_COMMAND_MODE;
                }
            }

            break;
        }

        case RTC_SYNC_WAIT_COMMAND_MODE:
        {
            if (strcmp(
                    message,
                    "REMOTE,MODE=COMMAND") == 0)
            {
                if (send_setdt_command(fd) == 0)
                {
                    rtc_sync_state =
                        RTC_SYNC_WAIT_SET_ACK;
                }
            }

            break;
        }

        case RTC_SYNC_WAIT_SET_ACK:
        {
            if (strncmp(
                    message,
                    "REMOTE,DATETIME=SET,",
                    20U) == 0)
            {
                if (serial_send_string(
                        fd,
                        "exit\r") == 0)
                {
                    rtc_sync_state =
                        RTC_SYNC_WAIT_STREAM_MODE;
                }
            }

            break;
        }

        case RTC_SYNC_WAIT_STREAM_MODE:
        {
            if (strcmp(
                    message,
                    "REMOTE,MODE=STREAM") == 0)
            {
                printf(
                    "STM32 RTC synchronisation complete\n");

                rtc_sync_state = RTC_SYNC_IDLE;
            }

            break;
        }

        default:
        {
            rtc_sync_state = RTC_SYNC_IDLE;
            break;
        }
    }
}
  
int main(void)
{
    int fd;
    unsigned char byte;

    printf("WaterGateway C receiver starting\n");
    printf("Opening %s at 38400 baud\n", ERIC_PORT);

    fd = open(ERIC_PORT, O_RDWR | O_NOCTTY);

    if (fd < 0)
    {
        perror("open");
        return EXIT_FAILURE;
    }

    if (configure_serial_port(fd) != 0)
    {
        close(fd);
        return EXIT_FAILURE;
    }

    printf("eRIC receiver ready\n");

    char line[256];
    size_t line_length = 0U;

    while (1)
    {
        ssize_t result = read(fd, &byte, 1);

        if (result < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            perror("read");
            break;
        }

        if (result != 1)
        {
            continue;
        }

        /*
         * Ignore carriage return.
         */
        if (byte == '\r')
        {
            continue;
        }

        /*
         * Newline marks the end of a complete message.
         */
        if (byte == '\n')
        {
            if (line_length > 0U)
            {
                line[line_length] = '\0';

		process_message(fd, line);

                line_length = 0U;
            }

            continue;
        }

        /*
         * Store normal characters while there is room.
         */
        if (line_length < (sizeof(line) - 1U))
        {
            line[line_length] = (char)byte;
            line_length++;
        }
        else
        {
            /*
             * Line overflow: discard the incomplete message.
             */
            fprintf(stderr, "Warning: received line too long\n");
            line_length = 0U;
        }
    }

    close(fd);

    return EXIT_SUCCESS;
}
