#ifndef PROTOCOL_H
#define PROTOCOL_H

void Protocol_Init(void);

void Protocol_ProcessMessage(
    int serial_fd,
    const char *message);

typedef struct
{
    int year;
    int month;
    int day;

    int hour;
    int minute;
    int second;

    float temperature_c;
    float humidity_percent;

    unsigned long water_frequency_hz;

} Telemetry_t;

int Protocol_ParseTelemetry(const char *message, Telemetry_t *telemetry);

#endif /* PROTOCOL_H */
