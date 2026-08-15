#ifndef STATUS_H
#define STATUS_H

#include "protocol.h"

#include <time.h>

typedef struct
{
    Telemetry_t telemetry;

    time_t last_received;

    unsigned long packets_received;
    unsigned long parse_errors;

    int telemetry_valid;

} GatewayStatus_t;

void Status_Init(void);

void Status_UpdateTelemetry(
    const Telemetry_t *telemetry);

void Status_RecordParseError(void);

const GatewayStatus_t *Status_Get(void);

int Status_TelemetryIsFresh(void);

#endif /* STATUS_H */
