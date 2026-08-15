#include "serial.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static int Serial_Configure(int fd)
{
    struct termios tty;

    if (tcgetattr(fd, &tty) != 0)
    {
        perror("tcgetattr");
        return -1;
    }

    cfmakeraw(&tty);

    cfsetispeed(&tty, B38400);
    cfsetospeed(&tty, B38400);

    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~CRTSCTS;

    /*
     * Return from read() after at most 100 ms so the main loop
     * can also service other tasks such as the HTTP server.
     */
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1;

    if (tcsetattr(fd, TCSANOW, &tty) != 0)
    {
        perror("tcsetattr");
        return -1;
    }

    return 0;
}

int Serial_Open(const char *device)
{
    int fd;

    if (device == NULL)
    {
        return -1;
    }

    fd = open(
        device,
        O_RDWR | O_NOCTTY);

    if (fd < 0)
    {
        perror("open");
        return -1;
    }

    if (Serial_Configure(fd) != 0)
    {
        close(fd);
        return -1;
    }

/*
 * Discard any stale receive data left in the UART/USB
 * buffers from a previous WaterGateway session.
 */
    if (tcflush(fd, TCIFLUSH) != 0)
    {
        perror("tcflush");
        close(fd);
        return -1;
    }

    return fd;
}

void Serial_Close(int fd)
{
    if (fd >= 0)
    {
        close(fd);
    }
}

int Serial_ReadByte(
    int fd,
    unsigned char *byte)
{
    ssize_t result;

    if ((fd < 0) || (byte == NULL))
    {
        return -1;
    }

    result = read(
        fd,
        byte,
        1U);

    if (result < 0)
    {
        if (errno == EINTR)
        {
            return 0;
        }

        perror("read");
        return -1;
    }

    return (result == 1) ? 1 : 0;
}

int Serial_WriteString(
    int fd,
    const char *text)
{
    size_t length;
    ssize_t result;

    if ((fd < 0) || (text == NULL))
    {
        return -1;
    }

    length = strlen(text);

    result = write(
        fd,
        text,
        length);

    if (result < 0)
    {
        perror("write");
        return -1;
    }

    if ((size_t)result != length)
    {
        fprintf(
            stderr,
            "Warning: incomplete serial write\n");

        return -1;
    }

    return 0;
}
