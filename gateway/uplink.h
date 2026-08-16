#ifndef UPLINK_H
#define UPLINK_H

#include "protocol.h"

int Uplink_SendTelemetry(
    const Telemetry_t *telemetry);

#endif
