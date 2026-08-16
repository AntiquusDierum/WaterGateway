#include "uplink.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define UPLINK_SERVER_ADDRESS "192.168.2.101"
#define UPLINK_SERVER_PORT 8090

int Uplink_SendTelemetry(
    const Telemetry_t *telemetry)
{
    int fd;
    struct sockaddr_in server;

    char body[256];
    char request[512];

    int body_length;
    int request_length;

    if (telemetry == NULL)
    {
        return -1;
    }

    body_length = snprintf(
        body,
        sizeof(body),
        "WB1,DATE=%04d-%02d-%02d,"
        "TIME=%02d:%02d:%02d,"
        "TEMP=%.1fC,"
        "HUM=%.1f%%,"
        "WATER=%luHz",
        telemetry->year,
        telemetry->month,
        telemetry->day,
        telemetry->hour,
        telemetry->minute,
        telemetry->second,
        telemetry->temperature_c,
        telemetry->humidity_percent,
        telemetry->water_frequency_hz);

    if ((body_length < 0) ||
        ((size_t)body_length >= sizeof(body)))
    {
        return -1;
    }

    request_length = snprintf(
        request,
        sizeof(request),
        "POST /telemetry HTTP/1.1\r\n"
        "Host: " UPLINK_SERVER_ADDRESS ":%d\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        UPLINK_SERVER_PORT,
        body_length,
        body);

    if ((request_length < 0) ||
        ((size_t)request_length >= sizeof(request)))
    {
        return -1;
    }

    fd = socket(
        AF_INET,
        SOCK_STREAM,
        0);

    if (fd < 0)
    {
        perror("uplink socket");
        return -1;
    }

    memset(
        &server,
        0,
        sizeof(server));

    server.sin_family = AF_INET;
    server.sin_port = htons(UPLINK_SERVER_PORT);

    if (inet_pton(
            AF_INET,
            UPLINK_SERVER_ADDRESS,
            &server.sin_addr) != 1)
    {
        close(fd);
        return -1;
    }

    if (connect(
            fd,
            (struct sockaddr *)&server,
            sizeof(server)) < 0)
    {
        perror("uplink connect");
        close(fd);
        return -1;
    }

    if (write(
            fd,
            request,
            (size_t)request_length) != request_length)
    {
        perror("uplink write");
        close(fd);
        return -1;
    }

    close(fd);

    return 0;
}
