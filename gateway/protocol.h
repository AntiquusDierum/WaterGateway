#ifndef PROTOCOL_H
#define PROTOCOL_H

void Protocol_Init(void);
void Protocol_Task(int serial_fd);

typedef enum
{
    PROTOCOL_RELAY_UNKNOWN = -1,
    PROTOCOL_RELAY_OFF = 0,
    PROTOCOL_RELAY_ON = 1

} ProtocolRelayState_t;

int Protocol_RequestRelay(int serial_fd, unsigned int relay_number,
    ProtocolRelayState_t state);

ProtocolRelayState_t Protocol_GetRelayState(unsigned int relay_number);

int Protocol_IsBusy(void);

void Protocol_ProcessMessage(int serial_fd, const char *message);

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

    ProtocolRelayState_t pump_state;
    int pump_lockout;

} Telemetry_t;

int Protocol_ParseTelemetry(const char *message, Telemetry_t *telemetry);

#endif /* PROTOCOL_H */
