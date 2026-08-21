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
#define RTC_SYNC_RETRY_SECONDS 60.0

typedef enum
{
    PROTOCOL_IDLE = 0,

    RTC_SYNC_WAIT_COMMAND_MODE,
    RTC_SYNC_WAIT_SET_ACK,
    RTC_SYNC_WAIT_STREAM_MODE,

    RELAY_WAIT_COMMAND_MODE,
    RELAY_WAIT_ACK,
    RELAY_WAIT_STREAM_MODE

} ProtocolState_t;

static ProtocolState_t protocol_state = PROTOCOL_IDLE;

static unsigned int pending_relay_number = 0U;
static ProtocolRelayState_t pending_relay_state =
    PROTOCOL_RELAY_UNKNOWN;

static ProtocolRelayState_t relay1_state =
    PROTOCOL_RELAY_UNKNOWN;

static ProtocolRelayState_t relay2_state =
    PROTOCOL_RELAY_UNKNOWN;

static time_t rtc_sync_started = 0;
static time_t rtc_sync_retry_after = 0;

static void protocol_set_state(ProtocolState_t new_state)
{
    protocol_state = new_state;

    if (new_state == PROTOCOL_IDLE)
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

int Protocol_RequestRelay(
    int serial_fd,
    unsigned int relay_number,
    ProtocolRelayState_t state)
{
    if ((relay_number < 1U) ||
        (relay_number > 2U))
    {
        return -1;
    }

    if ((state != PROTOCOL_RELAY_OFF) &&
        (state != PROTOCOL_RELAY_ON))
    {
        return -1;
    }

    /*
     * Only one remote transaction may use the STM32
     * command channel at a time.
     */
    if (protocol_state != PROTOCOL_IDLE)
    {
        return -1;
    }

    pending_relay_number = relay_number;
    pending_relay_state = state;

    if (Serial_WriteString(
            serial_fd,
            "\r") != 0)
    {
        pending_relay_number = 0U;
        pending_relay_state =
            PROTOCOL_RELAY_UNKNOWN;

        return -1;
    }

    protocol_set_state(
        RELAY_WAIT_COMMAND_MODE);

    return 0;
}

ProtocolRelayState_t Protocol_GetRelayState(
    unsigned int relay_number)
{
    switch (relay_number)
    {
        case 1U:
            return relay1_state;

        case 2U:
            return relay2_state;

        default:
            return PROTOCOL_RELAY_UNKNOWN;
    }
}

int Protocol_IsBusy(void)
{
    return (protocol_state != PROTOCOL_IDLE);
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
    
    switch (protocol_state)
    {
        case PROTOCOL_IDLE:
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
	        time_t now;

		now = time(NULL);

		if ((rtc_sync_retry_after == 0) ||
		    (now >= rtc_sync_retry_after))
		{
		    printf("STM32 RTC differs from Pi by %.1f seconds; "
			   "synchronising...\n",
			   difference);

		    if (Serial_WriteString(fd, "\r") == 0)
		    {
		        protocol_set_state(RTC_SYNC_WAIT_COMMAND_MODE);
		    }
		}
	    }

            break;
        }
	
	case RTC_SYNC_WAIT_COMMAND_MODE:
	{
	    if (strcmp(message,"REMOTE,MODE=COMMAND") == 0)
	    {
	        if (send_setdt_command(fd) == 0)
		{
		    protocol_set_state(RTC_SYNC_WAIT_SET_ACK);
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
		    protocol_set_state(RTC_SYNC_WAIT_STREAM_MODE);
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
		rtc_sync_retry_after = 0;
		protocol_set_state(PROTOCOL_IDLE);
            }

            break;
        }

        case RELAY_WAIT_COMMAND_MODE:
	{
	    if (strcmp(message, "REMOTE,MODE=COMMAND") == 0)
	    {
	        char command[32];

		snprintf(
			 command,
			 sizeof(command),
			 "relay %u %s\r",
			 pending_relay_number,
			 (pending_relay_state ==
			  PROTOCOL_RELAY_ON)
			 ? "on"
			 : "off");

		if (Serial_WriteString(fd, command) == 0)
		{
		    protocol_set_state(RELAY_WAIT_ACK);
		}
	    }

	    break;
	}

        case RELAY_WAIT_ACK:
	{
	    unsigned int relay_number;
	    char state[8];

	    if (sscanf(message,
		       "REMOTE,RELAY=%u,STATE=%7s",
		       &relay_number,
		       state) == 2)
	    {
	        if (relay_number == pending_relay_number)
		{
		    ProtocolRelayState_t confirmed_state;

		    if (strcmp(state, "ON") == 0)
		    {
		        confirmed_state = PROTOCOL_RELAY_ON;
		    }
		    else if (strcmp(state, "OFF") == 0)
		    {
		        confirmed_state =
			PROTOCOL_RELAY_OFF;
		    }
		    else
		    {
			break;
		    }

		    if (confirmed_state != pending_relay_state)
		    {
		        break;
		    }

		    if (relay_number == 1U)
		    {
		        relay1_state = confirmed_state;
		    }
		    else
		    {
		        relay2_state = confirmed_state;
		    }

		    printf(
			   "Relay %u confirmed %s\n",
			   relay_number,
			   (confirmed_state ==
			    PROTOCOL_RELAY_ON)
			   ? "ON"
			   : "OFF");

		    if (Serial_WriteString(fd, "exit\r") == 0)
		    {
		        protocol_set_state(RELAY_WAIT_STREAM_MODE);
		    }
		}
	    }

	    break;
	}

        case RELAY_WAIT_STREAM_MODE:
	{
	    if (strcmp(message, "REMOTE,MODE=STREAM") == 0)
	    {
	        pending_relay_number = 0U;
		pending_relay_state = PROTOCOL_RELAY_UNKNOWN;

		protocol_set_state(PROTOCOL_IDLE);

		printf("Relay command transaction complete\n");
	    }

	    break;
	}

        default:
        {
	    protocol_set_state(PROTOCOL_IDLE);

            break;
        }
    }
}

void Protocol_Init(void)
{
    protocol_set_state(PROTOCOL_IDLE);

    pending_relay_number = 0U;
    pending_relay_state =
        PROTOCOL_RELAY_UNKNOWN;

    relay1_state =
        PROTOCOL_RELAY_UNKNOWN;

    relay2_state =
        PROTOCOL_RELAY_UNKNOWN;
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

void Protocol_Task(int serial_fd)
{

    time_t now;
    double elapsed;
    int was_rtc_sync;

    if (protocol_state == PROTOCOL_IDLE)
    {
        return;
    }

    now = time(NULL);

    elapsed = difftime(now, rtc_sync_started);

    was_rtc_sync =
    (protocol_state == RTC_SYNC_WAIT_COMMAND_MODE) ||
    (protocol_state == RTC_SYNC_WAIT_SET_ACK) ||
    (protocol_state == RTC_SYNC_WAIT_STREAM_MODE);
    
    if (elapsed >= RTC_SYNC_TIMEOUT_SECONDS)
    {
        fprintf(stderr,
		"Remote command transaction timed out; "
		"forcing stream mode\n");

	if (Serial_WriteString(serial_fd,"exit\r") != 0)
	{
	    fprintf(stderr,
		    "Warning: unable to send stream-mode recovery command\n");
	}

	if (was_rtc_sync)
	{
	    rtc_sync_retry_after = time(NULL) + (time_t)RTC_SYNC_RETRY_SECONDS;
	}

	pending_relay_number = 0U;
	pending_relay_state = PROTOCOL_RELAY_UNKNOWN;

	protocol_set_state(PROTOCOL_IDLE);
    }
}
