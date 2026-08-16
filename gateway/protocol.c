#include "protocol.h"
#include "serial.h"
#include "logger.h"
#include "status.h"
#include "uplink.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define RTC_SYNC_TOLERANCE_SECONDS 10.0
#define RTC_SYNC_TIMEOUT_SECONDS 10.0

typedef enum
{
    RTC_SYNC_IDLE = 0,
    RTC_SYNC_WAIT_COMMAND_MODE,
    RTC_SYNC_WAIT_SET_ACK,
    RTC_SYNC_WAIT_STREAM_MODE

} RtcSyncState_t;

static RtcSyncState_t rtc_sync_state = RTC_SYNC_IDLE;
static time_t rtc_sync_started = 0;

static void rtc_sync_set_state(RtcSyncState_t new_state)
{
    rtc_sync_state = new_state;

    if (new_state == RTC_SYNC_IDLE)
    {
        rtc_sync_started = 0;
    }
    else
    {
        rtc_sync_started = time(NULL);
    }
}

static int parse_stm_datetime(const char *message, time_t *stm_time)
{
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;

    struct tm tm_value = {0};

    if ((message == NULL) || (stm_time == NULL))
    {
        return -1;
    }

    if (sscanf(message,"WB1,DATE=%d-%d-%d,TIME=%d:%d:%d",&year,&month,&day,&hou\
r,&minute,&second) != 6)
    {
        return -1;
    }

    tm_value.tm_year = year - 1900;
    tm_value.tm_mon = month - 1;
    tm_value.tm_mday = day;
    tm_value.tm_hour = hour;
    tm_value.tm_min = minute;
    tm_value.tm_sec = second;
    /* Let the C library determine whether daylight-saving
     * time applies to this local date/time. */
    tm_value.tm_isdst = -1;

    *stm_time = mktime(&tm_value);

    if (*stm_time == (time_t)-1)
    {
        return -1;
    }

    return 0;
}

static int telemetry_time_is_sane(
    const Telemetry_t *telemetry)
{
    struct tm tm_value = {0};
    time_t stm_time;
    time_t pi_time;
    double difference;

    if (telemetry == NULL)
    {
        return 0;
    }

    if (telemetry->year < 2026)
    {
        return 0;
    }

    tm_value.tm_year = telemetry->year - 1900;
    tm_value.tm_mon = telemetry->month - 1;
    tm_value.tm_mday = telemetry->day;
    tm_value.tm_hour = telemetry->hour;
    tm_value.tm_min = telemetry->minute;
    tm_value.tm_sec = telemetry->second;
    tm_value.tm_isdst = -1;

    stm_time = mktime(&tm_value);

    if (stm_time == (time_t)-1)
    {
        return 0;
    }

    pi_time = time(NULL);

    difference = difftime(
        pi_time,
        stm_time);

    if (difference < 0.0)
    {
        difference = -difference;
    }

    return (difference <= 60.0);
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

    return Serial_WriteString(fd, command);
}

void Protocol_ProcessMessage(int fd, const char *message)
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

    if (strncmp(message, "WB1,", 4U) == 0)
    {
        Telemetry_t telemetry;

	if (Protocol_ParseTelemetry(message, &telemetry) == 0)
	{
	    Status_UpdateTelemetry(&telemetry);
	    if (telemetry_time_is_sane(&telemetry))
	    {
	        if (Logger_LogTelemetry(&telemetry) != 0)
		{
		    fprintf(stderr, "Warning: telemetry logging failed\n");
		}
		if (Uplink_SendTelemetry(&telemetry) != 0)
		{
		    fprintf(stderr, "Warning: telemetry uplink failed\n");
		}
	    }
	}
	else
	{
	    Status_RecordParseError();
	}    
    }
    
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

                if (Serial_WriteString(fd, "\r") == 0)
                {
		    rtc_sync_set_state(RTC_SYNC_WAIT_COMMAND_MODE);
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
		    rtc_sync_set_state(RTC_SYNC_WAIT_SET_ACK);
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
                if (Serial_WriteString(fd, "exit\r") == 0)
                {
		    rtc_sync_set_state(RTC_SYNC_WAIT_STREAM_MODE);
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
		rtc_sync_set_state(RTC_SYNC_IDLE);
            }

            break;
        }

        default:
        {
	    rtc_sync_set_state(RTC_SYNC_IDLE);

            break;
        }
    }
}

void Protocol_Init(void)
{
    rtc_sync_set_state(RTC_SYNC_IDLE);
}

int Protocol_ParseTelemetry(
    const char *message,
    Telemetry_t *telemetry)
{
    if ((message == NULL) ||
        (telemetry == NULL))
    {
        return -1;
    }

    if (sscanf(
            message,
            "WB1,DATE=%d-%d-%d,"
            "TIME=%d:%d:%d,"
            "TEMP=%fC,"
            "HUM=%f%%,"
            "WATER=%luHz",
            &telemetry->year,
            &telemetry->month,
            &telemetry->day,
            &telemetry->hour,
            &telemetry->minute,
            &telemetry->second,
            &telemetry->temperature_c,
            &telemetry->humidity_percent,
            &telemetry->water_frequency_hz) != 9)
    {
        return -1;
    }

    return 0;
}

void Protocol_Task(void)
{

    time_t now;
    double elapsed;

    if (rtc_sync_state == RTC_SYNC_IDLE)
    {
        return;
    }

    now = time(NULL);

    elapsed = difftime(
        now,
        rtc_sync_started);

    if (elapsed >= RTC_SYNC_TIMEOUT_SECONDS)
    {
        fprintf(
            stderr,
            "RTC synchronisation timed out; "
            "returning to idle state\n");

        rtc_sync_set_state(
            RTC_SYNC_IDLE);
    }
}

