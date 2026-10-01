#!/usr/bin/env python3
"""Light all verified front packages white; led_discovery.py exits this preset."""

import sys

import hid

from led_discovery import find_module, PACKET_SIZE


def show_front():
    packet = bytearray(PACKET_SIZE)
    packet[0:2] = b"\x09\x7d"
    with hid.Device(path=find_module()["path"]) as device:
        device.write(b"\x00" + packet)
        response = device.read(PACKET_SIZE, timeout=2000)
    if len(response) != PACKET_SIZE or response[0] != 0 or response[1:6] != packet[1:6]:
        raise ValueError("No successful front-test response; flash the front-test discovery build.")
    print(f"Front test: {response[6]} verified LEDs, white RGB({response[7]}, {response[7]}, {response[7]}); all other addresses off.")
    print(f"Firmware address range: {response[11]}; use led_discovery.py 255 255 255 to turn off.")


if __name__ == "__main__":
    try:
        show_front()
    except (ValueError, hid.HIDException) as error:
        print(f"Front test: {error}", file=sys.stderr)
        sys.exit(1)
