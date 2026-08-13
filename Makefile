CC = gcc
CFLAGS = -Wall -Wextra -O2

TARGET = gateway/watergateway

OBJECTS = gateway/main.o \
          gateway/serial.o \
          gateway/protocol.o

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS)

gateway/main.o: gateway/main.c gateway/serial.h gateway/protocol.h
	$(CC) $(CFLAGS) -c gateway/main.c -o gateway/main.o

gateway/serial.o: gateway/serial.c gateway/serial.h
	$(CC) $(CFLAGS) -c gateway/serial.c -o gateway/serial.o

gateway/protocol.o: gateway/protocol.c gateway/protocol.h gateway/serial.h
	$(CC) $(CFLAGS) -c gateway/protocol.c -o gateway/protocol.o

clean:
	rm -f $(OBJECTS) $(TARGET)
