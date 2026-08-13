#ifndef PROTOCOL_H
#define PROTOCOL_H

void Protocol_Init(void);

void Protocol_ProcessMessage(
    int serial_fd,
    const char *message);

#endif /* PROTOCOL_H */
