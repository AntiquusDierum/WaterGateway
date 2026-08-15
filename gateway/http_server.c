#include "http_server.h"
#include "status.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define HTTP_PORT 8080

static int listen_fd = -1;


static int set_nonblocking(int fd)
{
    int flags;

    flags = fcntl(fd, F_GETFL, 0);

    if (flags < 0)
    {
        return -1;
    }

    if (fcntl(
            fd,
            F_SETFL,
            flags | O_NONBLOCK) < 0)
    {
        return -1;
    }

    return 0;
}


int HttpServer_Init(void)
{
    struct sockaddr_in address;
    int option = 1;

    listen_fd = socket(
        AF_INET,
        SOCK_STREAM,
        0);

    if (listen_fd < 0)
    {
        perror("socket");
        return -1;
    }

    if (setsockopt(
            listen_fd,
            SOL_SOCKET,
            SO_REUSEADDR,
            &option,
            sizeof(option)) < 0)
    {
        perror("setsockopt");
        close(listen_fd);
        listen_fd = -1;
        return -1;
    }

    memset(
        &address,
        0,
        sizeof(address));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(HTTP_PORT);

    if (bind(
            listen_fd,
            (struct sockaddr *)&address,
            sizeof(address)) < 0)
    {
        perror("bind");
        close(listen_fd);
        listen_fd = -1;
        return -1;
    }

    if (listen(
            listen_fd,
            4) < 0)
    {
        perror("listen");
        close(listen_fd);
        listen_fd = -1;
        return -1;
    }

    if (set_nonblocking(listen_fd) != 0)
    {
        perror("fcntl");
        close(listen_fd);
        listen_fd = -1;
        return -1;
    }

    printf(
        "HTTP status server listening on port %d\n",
        HTTP_PORT);

    return 0;
}


void HttpServer_Task(void)
{
    int client_fd;
    char request[512];
    char response[2048];

    const GatewayStatus_t *status;

    time_t now;
    double age;

    const char *state;

    if (listen_fd < 0)
    {
        return;
    }

    client_fd = accept(
        listen_fd,
        NULL,
        NULL);

    if (client_fd < 0)
    {
        if ((errno == EAGAIN) ||
            (errno == EWOULDBLOCK))
        {
            return;
        }

        perror("accept");
        return;
    }

    /*
     * We don't need to interpret the request yet.
     * Reading it simply consumes the browser's HTTP request.
     */
    (void)read(
        client_fd,
        request,
        sizeof(request));

    status = Status_Get();

    if (Status_TelemetryIsFresh())
    {
        state = "ONLINE";
    }
    else
    {
        state = "STALE";
    }

    if (status->telemetry_valid)
    {
        now = time(NULL);

        age = difftime(
            now,
            status->last_received);

        snprintf(
            response,
            sizeof(response),

            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "Connection: close\r\n"
            "\r\n"

            "<!DOCTYPE html>"
            "<html>"
            "<head>"
            "<title>WaterGateway</title>"
            "<meta http-equiv=\"refresh\" content=\"5\">"
            "</head>"
            "<body>"
            "<h1>WaterGateway</h1>"
            "<p><strong>Status:</strong> %s</p>"
            "<p><strong>Last packet:</strong> %.0f seconds ago</p>"
            "<hr>"
            "<p><strong>Temperature:</strong> %.1f &deg;C</p>"
            "<p><strong>Humidity:</strong> %.1f %%</p>"
            "<p><strong>Water:</strong> %lu Hz</p>"
            "<hr>"
            "<p><strong>Packets received:</strong> %lu</p>"
            "<p><strong>Parse errors:</strong> %lu</p>"
            "</body>"
            "</html>",

            state,
            age,
            status->telemetry.temperature_c,
            status->telemetry.humidity_percent,
            status->telemetry.water_frequency_hz,
            status->packets_received,
            status->parse_errors);
    }
    else
    {
        snprintf(
            response,
            sizeof(response),

            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "Connection: close\r\n"
            "\r\n"

            "<!DOCTYPE html>"
            "<html>"
            "<head>"
            "<title>WaterGateway</title>"
            "</head>"
            "<body>"
            "<h1>WaterGateway</h1>"
            "<p><strong>Status:</strong> NO TELEMETRY</p>"
            "</body>"
            "</html>");
    }

    (void)write(
        client_fd,
        response,
        strlen(response));

    close(client_fd);
}


void HttpServer_Close(void)
{
    if (listen_fd >= 0)
    {
        close(listen_fd);
        listen_fd = -1;
    }
}
