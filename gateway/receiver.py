#!/usr/bin/env python3

from datetime import datetime
import serial


ERIC_PORT = "/dev/serial/by-id/usb-FTDI_FT231X_USB_UART_DM03PVN2-if00-port0"
ERIC_BAUD = 38400


def main():
    print("WaterGateway receiver starting")
    print(f"Opening {ERIC_PORT} at {ERIC_BAUD} baud")

    with serial.Serial(
        port=ERIC_PORT,
        baudrate=ERIC_BAUD,
        bytesize=serial.EIGHTBITS,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        timeout=1.0,
    ) as eric:

        print("eRIC receiver ready")

        while True:
            raw = eric.readline()

            if not raw:
                continue

            message = raw.decode("utf-8", errors="replace").strip()

            if not message:
                continue

            timestamp = datetime.now().astimezone().isoformat(timespec="milliseconds")

            print(f"{timestamp}  {message}", flush=True)


if __name__ == "__main__":
    main()
    
