#!/usr/bin/env python3
"""Light every configured LED address white, including front, badge and back."""

import sys

import hid

from led_discovery import find_module, PACKET_SIZE


def show_all():
    packet = bytearray(PACKET_SIZE)
    packet[0:2] = b"\x09\x7e"
    with hid.Device(path=find_module()["path"]) as device:
        device.write(b"\x00" + packet)
        response = device.read(PACKET_SIZE, timeout=2000)
    if len(response) != PACKET_SIZE or response[0] != 0 or response[1:6] != packet[1:6]:
        raise ValueError("No successful all-address response; flash the all-address discovery build.")
    print(f"All-address test: indices 0–{response[6] - 1}, white RGB({response[7]}, {response[7]}, {response[7]}).")
    print(f"Firmware address count: {response[11]}; use led_discovery.py 255 255 255 to turn off.")


if __name__ == "__main__":
    try:
        show_all()
    except (ValueError, hid.HIDException) as error:
        print(f"All-address test: {error}", file=sys.stderr)
        sys.exit(1)
