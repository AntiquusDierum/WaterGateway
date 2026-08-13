#ifndef SERIAL_H
#define SERIAL_H

int Serial_Open(const char *device);
void Serial_Close(int fd);

int Serial_ReadByte(
    int fd,
    unsigned char *byte);

int Serial_WriteString(
    int fd,
    const char *text);

#endif /* SERIAL_H */
