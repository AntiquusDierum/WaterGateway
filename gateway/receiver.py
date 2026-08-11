#!/usr/bin/env python3

from datetime import datetime
import serial

ERIC_PORT = "/dev/serial/by-id/usb-FTDI_FT231X_USB_UART_DM03PVN2-if00-port0"
ERIC_BAUD = 38400
RTC_SYNC_TOLERANCE_SECONDS = 10
RTC_SYNC_MIN_YEAR = 2026

RTC_SYNC_IDLE = 0
RTC_SYNC_WAIT_COMMAND_MODE = 1
RTC_SYNC_WAIT_SET_ACK = 2
RTC_SYNC_WAIT_STREAM_MODE = 3

def parse_stm_datetime(message):
    """Return the STM32 datetime from a WB1 telemetry message."""
    if not message.startswith("WB1,"):
        return None

    fields = {}

    for item in message.split(",")[1:]:
        if "=" not in item:
            continue

        key, value = item.split("=", 1)
        fields[key] = value

    if "DATE" not in fields or "TIME" not in fields:
        return None

    try:
        return datetime.strptime(
            f"{fields['DATE']} {fields['TIME']}",
            "%Y-%m-%d %H:%M:%S",
        ).astimezone()

    except ValueError:
        return None


    
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

        rtc_sync_state = RTC_SYNC_IDLE
        
        try:
            while True:
                raw = eric.readline()

                if not raw:
                    continue

                message = raw.decode("utf-8", errors="replace").strip()

                if not message:
                    continue

                print(message, flush=True)

                stm_datetime = parse_stm_datetime(message)

                if rtc_sync_state == RTC_SYNC_IDLE:
                    if stm_datetime is not None:
                        pi_now = datetime.now().astimezone()

                        if pi_now.year >= RTC_SYNC_MIN_YEAR:
                            difference = abs(
                                (pi_now - stm_datetime).total_seconds()
                            )

                            if difference > RTC_SYNC_TOLERANCE_SECONDS:
                                print(
                                    f"STM32 RTC differs from Pi by "
                                    f"{difference:.1f} seconds; "
                                    f"synchronising..."
                                )

                                eric.write(b"\r")
                                eric.flush()

                                rtc_sync_state = (
                                    RTC_SYNC_WAIT_COMMAND_MODE
                                )

                elif rtc_sync_state == RTC_SYNC_WAIT_COMMAND_MODE:
                    if message == "REMOTE,MODE=COMMAND":
                        pi_now = datetime.now().astimezone()

                        command = pi_now.strftime(
                            "setdt %Y-%m-%d %H:%M:%S\r"
                        )

                        eric.write(command.encode("ascii"))
                        eric.flush()

                        rtc_sync_state = RTC_SYNC_WAIT_SET_ACK

                elif rtc_sync_state == RTC_SYNC_WAIT_SET_ACK:
                    if message.startswith(
                        "REMOTE,DATETIME=SET,"
                    ):
                        eric.write(b"exit\r")
                        eric.flush()

                        rtc_sync_state = (
                            RTC_SYNC_WAIT_STREAM_MODE
                        )

                elif rtc_sync_state == RTC_SYNC_WAIT_STREAM_MODE:
                    if message == "REMOTE,MODE=STREAM":
                        print(
                            "STM32 RTC synchronisation complete"
                        )

                        rtc_sync_state = RTC_SYNC_IDLE
                    
        except KeyboardInterrupt:
            print(
                "\nWaterGateway receiver stopped"
            )

if __name__ == "__main__":
    main()
    
