#include "protocol.h"
#include "serial.h"
#include "logger.h"
#include "status.h"
#include "http_server.h"

#include <stdio.h>
#include <stdlib.h>

#define ERIC_PORT \
    "/dev/serial/by-id/usb-FTDI_FT231X_USB_UART_DM03PVN2-if00-port0"

#define LINE_BUFFER_SIZE 256

int main(void)
{
    int fd;

    unsigned char byte;

    char line[LINE_BUFFER_SIZE];
    size_t line_length = 0U;

    printf(
        "WaterGateway C receiver starting\n");

    printf(
        "Opening %s at 38400 baud\n",
        ERIC_PORT);

    fd = Serial_Open(ERIC_PORT);

    if (fd < 0)
    {
        return EXIT_FAILURE;
    }

    Protocol_Init();
    Status_Init();

    if (HttpServer_Init() != 0)
    {
        fprintf(stderr,"Warning: HTTP server initialisation failed\n");
    }

    if (Logger_Init() != 0)
    {
	fprintf(stderr, "Warning: logger initialisation failed\n");
    }
    
    printf(
        "eRIC receiver ready\n");

    while (1)
    {
        int result;

	Protocol_Task(fd);
	HttpServer_Task();
	
        result = Serial_ReadByte(
            fd,
            &byte);

        if (result < 0)
        {
            break;
        }

        if (result == 0)
        {
            continue;
        }

        if (byte == '\r')
        {
            continue;
        }

        if (byte == '\n')
        {
            if (line_length > 0U)
            {
                line[line_length] = '\0';

                Protocol_ProcessMessage(
                    fd,
                    line);

                line_length = 0U;
            }

            continue;
        }

        if (line_length <
            (sizeof(line) - 1U))
        {
            line[line_length] =
                (char)byte;

            line_length++;
        }
        else
        {
            fprintf(
                stderr,
                "Warning: received line too long\n");

            line_length = 0U;
        }
    }

    HttpServer_Close();
    
    Serial_Close(fd);

    return EXIT_FAILURE;
}
