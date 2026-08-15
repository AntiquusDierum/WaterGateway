#ifndef LOGGER_H
#define LOGGER_H

#include "protocol.h"

int Logger_Init(void);

int Logger_LogTelemetry(
    const Telemetry_t *telemetry);

#endif /* LOGGER_H */
