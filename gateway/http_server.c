#include "http_server.h"
#include "status.h"
#include "protocol.h"

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


void HttpServer_Task(int serial_fd)
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
     * The accepted connection must also be non-blocking.
     * Otherwise an idle browser connection could stall the
     * whole WaterGateway application.
     */
    if (set_nonblocking(client_fd) != 0)
    {
	perror("fcntl");

	close(client_fd);
	return;
    }
    /*
     * We don't need to interpret the request yet.
     * Reading it simply consumes the browser's HTTP request.
     */
    ssize_t request_length;

    request_length = read(client_fd,request,sizeof(request));

    if (request_length < 0)
    {
        if ((errno == EAGAIN) ||
            (errno == EWOULDBLOCK))
	{
	    close(client_fd);
            return;
	}

	perror("read");
	close(client_fd);
	return;
    }

    if (request_length == 0)
    {
        close(client_fd);
	return;
    }

    if ((size_t)request_length >= sizeof(request))
    {
        request_length = sizeof(request) - 1;
    }

    request[request_length] = '\0';

    if (strncmp(request, "GET /relay/1/on ", 16U) == 0)
    {
        (void)Protocol_RequestRelay(serial_fd, 1U, PROTOCOL_RELAY_ON);

	snprintf(response,
		 sizeof(response),
		 "HTTP/1.1 303 See Other\r\n"
		 "Location: /\r\n"
		 "Connection: close\r\n"
		 "\r\n");

	(void)write(client_fd, response, strlen(response));

	close(client_fd);

	return;
    }

    if (strncmp(request, "GET /relay/1/off ", 17U) == 0)
    {
        (void)Protocol_RequestRelay(serial_fd, 1U, PROTOCOL_RELAY_OFF);

	snprintf(response, sizeof(response),
		 "HTTP/1.1 303 See Other\r\n"
		 "Location: /\r\n"
		 "Connection: close\r\n"
		 "\r\n");

	(void)write(client_fd, response, strlen(response));

	close(client_fd);

	return;
    }
    
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
	    "<style>"
	    "* { box-sizing: border-box; }"

	    "body {"
	    "  font-family: sans-serif;"
	    "  margin: 0;"
	    "  padding: 32px;"
	    "  background: #111;"
	    "  color: #eee;"
	    "  text-align: center;"
	    "}"

	    "h1 {"
	    "  font-size: 4em;"
	    "  letter-spacing: 0.08em;"
	    "  margin: 20px 0 32px 0;"
	    "}"

	    ".panel {"
	    "  background: #222;"
	    "  padding: 28px 20px;"
	    "  margin-bottom: 24px;"
	    "  border-radius: 16px;"
	    "}"

	    ".label {"
	    "  display: block;"
	    "  width: auto;"
	    "  font-size: 0.45em;"
	    "  font-weight: normal;"
	    "  text-transform: uppercase;"
	    "  letter-spacing: 0.12em;"
	    "  margin-top: 8px;"
	    "}"

	    ".reading {"
	    "  font-size: 3.2em;"
	    "  font-weight: bold;"
	    "  margin: 32px 0;"
	    "}"

	    ".status {"
	    "  font-size: 2em;"
	    "  font-weight: bold;"
	    "}"
	    "</style>"
	    "</head>"
	    "<body>"

	    "<h1>WaterGateway</h1>"

	    "<div class=\"panel\">"
	    "<div><span class=\"label\">Status:</span>"
	    "<span class=\"status\">%s</span></div>"
	    "<div><span class=\"label\">Last packet:</span>"
	    "%.0f seconds ago</div>"
	    "</div>"

	    "<div class=\"panel\">"
	    "<div class=\"reading\"><span class=\"label\">Temperature:</span>"
	    "%.1f &deg;C</div>"
	    "<div class=\"reading\"><span class=\"label\">Humidity:</span>"
	    "%.1f %%</div>"
	    "<div class=\"reading\"><span class=\"label\">Water sensor frequency:</span>"
	    "%lu Hz</div>"
	    "</div>"

	    "<div class=\"panel\">"
	    "<div><span class=\"label\">Packets received:</span>"
	    "%lu</div>"
	    "<div><span class=\"label\">Parse errors:</span>"
	    "%lu</div>"
	    "</div>"

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
	    "<meta http-equiv=\"refresh\" content=\"5\">"
	    "</head>"
	    "<body>"
	    "<h1>WaterGateway</h1>"
	    "<p><strong>Status:</strong> NO TELEMETRY</p>"
	    "</body>"
	    "</html>"
		 );
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
