#include "uplink.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define UPLINK_SERVER_ADDRESS "192.168.2.101"
#define UPLINK_SERVER_PORT 8090
#define UPLINK_CONNECT_TIMEOUT_MS 1000

static int Uplink_ConnectWithTimeout(
    int fd,
    const struct sockaddr *address,
    socklen_t address_length,
    int timeout_ms)
{
    int flags;
    int result;
    int socket_error;
    socklen_t socket_error_length;
    struct pollfd poll_fd;

    flags = fcntl(fd, F_GETFL, 0);

    if (flags < 0)
    {
        perror("uplink fcntl get");
        return -1;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        perror("uplink fcntl set");
        return -1;
    }

    result = connect(
        fd,
        address,
        address_length);

    if (result == 0)
    {
        /*
         * Connected immediately.
         */
        (void)fcntl(fd, F_SETFL, flags);
        return 0;
    }

    if (errno != EINPROGRESS)
    {
        perror("uplink connect");
        (void)fcntl(fd, F_SETFL, flags);
        return -1;
    }

    poll_fd.fd = fd;
    poll_fd.events = POLLOUT;
    poll_fd.revents = 0;

    result = poll(
        &poll_fd,
        1,
        timeout_ms);

    if (result == 0)
    {
        fprintf(
            stderr,
            "uplink connect: timed out after %d ms\n",
            timeout_ms);

        (void)fcntl(fd, F_SETFL, flags);
        return -1;
    }

    if (result < 0)
    {
        perror("uplink poll");
        (void)fcntl(fd, F_SETFL, flags);
        return -1;
    }

    socket_error = 0;
    socket_error_length = sizeof(socket_error);

    if (getsockopt(
            fd,
            SOL_SOCKET,
            SO_ERROR,
            &socket_error,
            &socket_error_length) < 0)
    {
        perror("uplink getsockopt");
        (void)fcntl(fd, F_SETFL, flags);
        return -1;
    }

    if (socket_error != 0)
    {
        errno = socket_error;
        perror("uplink connect");

        (void)fcntl(fd, F_SETFL, flags);
        return -1;
    }

    /*
     * Restore normal blocking operation once connected.
     */
    if (fcntl(fd, F_SETFL, flags) < 0)
    {
        perror("uplink fcntl restore");
        return -1;
    }

    return 0;
}

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

    if (Uplink_ConnectWithTimeout(fd,(struct sockaddr *)&server,
	sizeof(server),UPLINK_CONNECT_TIMEOUT_MS) != 0)
    {
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
