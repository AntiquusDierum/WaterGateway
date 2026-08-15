#include "status.h"

#include <string.h>
#include <time.h>

#define TELEMETRY_STALE_SECONDS 15

static GatewayStatus_t gateway_status;

void Status_Init(void)
{
    memset(
        &gateway_status,
        0,
        sizeof(gateway_status));
}

void Status_UpdateTelemetry(
    const Telemetry_t *telemetry)
{
    if (telemetry == NULL)
    {
        return;
    }

    gateway_status.telemetry = *telemetry;

    gateway_status.last_received = time(NULL);

    gateway_status.packets_received++;

    gateway_status.telemetry_valid = 1;
}

void Status_RecordParseError(void)
{
    gateway_status.parse_errors++;
}

const GatewayStatus_t *Status_Get(void)
{
    return &gateway_status;
}

int Status_TelemetryIsFresh(void)
{
    time_t now;
    double age;

    if (!gateway_status.telemetry_valid)
    {
        return 0;
    }

    now = time(NULL);

    age = difftime(
        now,
        gateway_status.last_received);

    return (age <= TELEMETRY_STALE_SECONDS);
}
