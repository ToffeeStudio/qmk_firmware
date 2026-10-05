#!/usr/bin/env python3
"""Blank Module LEDs every 200 ms and request verified, saved zero brightness."""

import argparse
from collections import deque
import secrets
import struct
import sys
import time

import hid

from led_discovery import PACKET_SIZE, PID, RAW_USAGE, RAW_USAGE_PAGE, VID

INTERVAL = 0.2
COMMAND = 0x7C
OFF_INDICES = b"\xff" * 5
SAVE_COMMAND = 0x8B


def off_report(packet_id):
    # HIDAPI needs report ID zero BEFORE the firmware's 32-byte report.
    packet = struct.pack("<BBI", 0x09, COMMAND, packet_id) + OFF_INDICES
    return b"\x00" + packet.ljust(PACKET_SIZE, b"\x00")


def save_off_report(packet_id):
    packet = struct.pack("<BBI", 0x09, SAVE_COMMAND, packet_id) + b"ZERO"
    return b"\x00" + packet.ljust(PACKET_SIZE, b"\x00")


def module_path():
    devices = [info for info in hid.enumerate(VID, PID)
               if info["usage_page"] == RAW_USAGE_PAGE and info["usage"] == RAW_USAGE]
    if len(devices) > 1:
        raise ValueError("Connect only one Module PCB; no target was selected.")
    return devices[0]["path"] if devices else None


def receive_ack(device, pending, save_pending):
    """Drain without delaying the next off command; ignore other HID traffic."""
    acknowledged = False
    saved = False
    unsupported = False
    for _ in range(32):
        reply = device.read(PACKET_SIZE, timeout=0)
        if not reply:
            break
        if len(reply) != PACKET_SIZE or reply[26] != 0xA5:
            continue
        packet_id = int.from_bytes(reply[28:32], "little")
        if reply[27] == SAVE_COMMAND and packet_id in save_pending:
            save_pending.remove(packet_id)
            if reply[0] == 0xEF:
                unsupported = True
                continue
            if reply[0] != 0 or reply[1:6] != bytes((1, 7, 0, 0, 0)):
                raise ValueError("Permanent reset NOT verified; firmware/storage error. LEDs were blanked; saved state may be unchanged.")
            saved = True
            continue
        if reply[27] != COMMAND:
            continue
        if packet_id not in pending:
            continue
        if reply[0] != 0:
            raise ValueError(f"Firmware rejected all-off (status 0x{reply[0]:02X}); unplug the PCB.")
        pending.remove(packet_id)
        acknowledged = True
    return acknowledged, saved, unsupported


def rescue():
    device = None
    path = None
    pending = deque(maxlen=32)
    save_pending = deque(maxlen=32)
    saved = False
    unsupported = False
    packet_id = secrets.randbits(32)
    last_ack = None
    last_log = 0.0
    sends = 0
    notice = ""
    print("ARMED: waiting for one Module 1067:626D / FF60:61; all-off every 200 ms.", flush=True)
    print("Close the configurator and other lighting scripts. Saves front/rear/badge brightness zero once supported firmware verifies storage.", flush=True)
    try:
        while True:
            started = time.monotonic()
            try:
                found = module_path()
                if device is not None and found != path:
                    device.close()
                    device = None
                    last_ack = None
                    pending.clear()
                    save_pending.clear()
                    saved = unsupported = False
                if found is None:
                    if notice != "waiting":
                        print("Waiting for USB. If the PCB shut down, unplug it and let it cool.", flush=True)
                        notice = "waiting"
                else:
                    if device is None:
                        device = hid.Device(path=found)
                        path = found
                        pending.clear()
                        last_ack = None
                        saved = unsupported = False
                    # First write precedes any reply waits, capability queries or logging.
                    packet_id = (packet_id + 1) & 0xFFFFFFFF
                    report = off_report(packet_id)
                    written = device.write(report)
                    if written != len(report):
                        raise hid.HIDException(f"Short HID write: {written}/{len(report)} bytes")
                    pending.append(packet_id)
                    sends += 1
                    acknowledged, saved_now, unsupported_now = receive_ack(device, pending, save_pending)
                    if acknowledged:
                        last_ack = time.monotonic()
                    if saved_now and not saved:
                        saved = True
                        print("SAVED OFF VERIFIED: front/rear/badge brightness zero in storage. Colours/effects/per-key settings preserved. Physical cold restart not yet checked.", flush=True)
                    if unsupported_now and not unsupported:
                        unsupported = True
                        print("FIRMWARE UPDATE REQUIRED: only temporary all-off is active; saved brightness is NOT reset. No unsafe slider fallback will be attempted.", flush=True)
                    if last_ack is not None and not saved and not unsupported:
                        packet_id = (packet_id + 1) & 0xFFFFFFFF
                        report = save_off_report(packet_id)
                        written = device.write(report)
                        if written != len(report):
                            raise hid.HIDException(f"Short save-off write: {written}/{len(report)} bytes")
                        save_pending.append(packet_id)
                    status = "acknowledged" if last_ack is not None and time.monotonic() - last_ack < 2 else "unconfirmed"
                    if status != notice or started - last_log >= 5:
                        message = ("All-off acknowledged; continuing every 200 ms."
                                   if status == "acknowledged" else
                                   "All-off sent; awaiting a matching reply. Recovery NOT confirmed.")
                        print(f"{message} Writes: {sends}", flush=True)
                        notice = status
                        last_log = started
            except hid.HIDException as error:
                if device is not None:
                    device.close()
                    device = None
                pending.clear()
                save_pending.clear()
                saved = unsupported = False
                last_ack = None
                if notice != "usb-error":
                    print(f"USB unavailable: {error}. Retrying every 200 ms; unplug if cycling.", flush=True)
                    notice = "usb-error"
            time.sleep(max(0, started + INTERVAL - time.monotonic()))
    except KeyboardInterrupt:
        print("\nStopped. No command to restore brightness was sent.", flush=True)
        print("Saved zero brightness was verified for this connection." if saved else
              "Permanent reset NOT confirmed; saved startup may still be unsafe.", flush=True)
    finally:
        if device is not None:
            device.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dry-run", action="store_true", help="Print the off packet without enumerating/opening USB.")
    args = parser.parse_args()
    if args.dry_run:
        print(f"Target: {VID:04X}:{PID:04X}, usage {RAW_USAGE_PAGE:04X}:{RAW_USAGE:02X}; interval {INTERVAL:.3f}s")
        print(f"HID report ({len(off_report(1))} bytes): {off_report(1).hex(' ')}")
        print(f"Save-off report: {save_off_report(2).hex(' ')}")
        print("Live run first blanks, then requests verified saved zeros. No firmware flash, keymap or macro changes.")
        return
    rescue()


if __name__ == "__main__":
    try:
        main()
    except (ValueError, hid.HIDException) as error:
        print(f"Lighting rescue: {error}", file=sys.stderr)
        sys.exit(1)
