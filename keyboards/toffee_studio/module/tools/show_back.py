#!/usr/bin/env python3
"""Run the rear-only XY rainbow; --phase freezes it for camera verification."""

import argparse
import sys

import hid

from led_discovery import find_module, PACKET_SIZE


def show_back(phase):
    if phase is not None and not 0 <= phase <= 255:
        raise ValueError("Provide a hue phase from 0 to 255.")
    packet = bytearray(PACKET_SIZE)
    packet[0:2] = b"\x09\x7f"
    packet[6] = phase is None
    packet[7] = 0 if phase is None else phase
    with hid.Device(path=find_module()["path"]) as device:
        device.write(b"\x00" + packet)
        response = device.read(PACKET_SIZE, timeout=2000)
    if len(response) != PACKET_SIZE or response[0] != 0 or response[1:6] != packet[1:6]:
        raise ValueError("No successful rear-test response; flash the rear-test discovery build.")
    mode = "cycling (10.24 seconds)" if phase is None else f"frozen phase {phase}"
    print(f"Rear XY rainbow: {response[6]} LEDs, drive {response[7]}/255, {mode}; all other addresses off.")
    print(f"Configured addresses: {response[11]}. Use led_discovery.py 255 255 255 to turn off.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--phase", type=int, help="Freeze at a hue phase 0–255 instead of cycling.")
    args = parser.parse_args()
    try:
        show_back(args.phase)
    except (ValueError, hid.HIDException) as error:
        print(f"Rear test: {error}", file=sys.stderr)
        sys.exit(1)
