import hid
import sys
import time
import argparse
import colorsys

# --- Configuration ---
VENDOR_ID = 0x1067
PRODUCT_ID = 0x626D
USAGE_PAGE = 0xFF60
USAGE = 0x61

# --- Command Constants ---
# These now reflect the unified command set in module_raw_hid.h
MAGIC = 0x09

# Filesystem Commands
ID_LS = 0x50
ID_FORMAT_FS = 0x5A
ID_PLAY_IMAGE = 0x5C
ID_LS_NEXT = 0x60

# --- NEW SEPARATED LIGHTING COMMANDS ---
ID_SET_ANIMATION = 0x71
ID_SET_SPEED = 0x72
ID_SET_BRIGHTNESS = 0x74
ID_SET_COLOR_HS = 0x75

# Return Codes from Device
RET_SUCCESS = 0x00
RET_MORE_ENTRIES = 0xEA
RET_INVALID_COMMAND = 0xEF

def find_device():
    """Finds the keyboard's raw HID interface."""
    for device_info in hid.enumerate():
        if (
            device_info["vendor_id"] == VENDOR_ID
            and device_info["product_id"] == PRODUCT_ID
            and device_info["usage_page"] == USAGE_PAGE
            and device_info["usage"] == USAGE
        ):
            return device_info["path"]
    return None

def send_command(device_path, command_id, payload=None):
    """Sends a command with an optional payload to the keyboard and doesn't wait for a reply."""
    if not device_path:
        print("Error: Keyboard not found.", file=sys.stderr)
        return False
    if payload is None:
        payload = []

    try:
        with hid.Device(path=device_path) as h:
            # Construct the 65-byte packet (1-byte Report ID 0x00 + 64 bytes data)
            packet = [0x00] * 65
            packet[1] = MAGIC
            packet[2] = command_id
            # Bytes 3-6 (indices) are unused packet ID, leave as 0

            # Copy payload into the packet starting at byte index 7 (firmware sees this as data[6])
            for i, val in enumerate(payload):
                if 7 + i < len(packet):
                    packet[7 + i] = val

            # The hid library prepends the report ID, so we send 64 bytes from our buffer.
            h.write(bytes(packet[1:]))
            return True

    except Exception as e:
        print(f"An error occurred during send: {e}", file=sys.stderr)
        return False

def send_and_receive_command(device_path, command_id, payload=None, timeout_ms=1000):
    """Sends a command, waits for, and returns the reply."""
    if not device_path:
        print("Error: Keyboard not found.", file=sys.stderr)
        return None
    if payload is None:
        payload = []

    try:
        with hid.Device(path=device_path) as h:
            packet = [0x00] * 65
            packet[1] = MAGIC
            packet[2] = command_id

            for i, val in enumerate(payload):
                if 7 + i < len(packet):
                    packet[7 + i] = val

            h.write(bytes(packet[1:]))

            # Read the response
            response = h.read(64, timeout=timeout_ms)
            if not response:
                print("Error: No response from device (timeout).", file=sys.stderr)
                return None

            return response

    except Exception as e:
        print(f"An error occurred during send/receive: {e}", file=sys.stderr)
        return None

def list_files(device_path):
    """Lists files on the device's filesystem by handling paged responses."""
    print("Listing files on device...")

    # Initial 'ls' command
    response = send_and_receive_command(device_path, ID_LS)
    if not response:
        return

    filenames = []

    while response:
        return_code = response[0]
        # The payload is the rest of the packet
        payload = bytes(response[1:])

        # Parse null-separated strings from payload
        current_list = payload.decode('utf-8', errors='ignore').split('\0')
        # Filter out any empty strings that result from parsing
        filenames.extend([f for f in current_list if f])

        if return_code == RET_MORE_ENTRIES:
            # More entries exist on the device, send the 'ls_next' command to get them
            response = send_and_receive_command(device_path, ID_LS_NEXT)
        elif return_code == RET_SUCCESS:
            # This was the last page of files
            break
        else:
            print(f"Error: Received unexpected return code 0x{return_code:02X}", file=sys.stderr)
            break

    if not filenames:
        print("(No files found)")
    else:
        for name in sorted(filenames):
             print(f"- {name.strip()}")

def rgb_to_hsv(r, g, b):
    """Converts RGB (0-255) to HSV (0-255 for QMK)"""
    # colorsys expects values between 0 and 1
    h, s, v = colorsys.rgb_to_hsv(r / 255.0, g / 255.0, b / 255.0)
    # QMK HSV uses 0-255 for all components
    return int(h * 255), int(s * 255), int(v * 255)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Control Toffee Studio Module features (lighting and filesystem).")

    # Lighting arguments
    lighting_group = parser.add_argument_group('Lighting Control')
    lighting_group.add_argument("--anim", type=int, help="Set lighting animation ID (0-9).")
    lighting_group.add_argument("--rgb", type=str, help="Set lighting color as R,G,B (e.g., '255,0,128'). Sets both color and brightness.")
    lighting_group.add_argument("--brightness", type=int, help="Set lighting brightness (0-255) independently.")
    lighting_group.add_argument("--speed", type=int, help="Set lighting animation speed (0-255).")

    # Filesystem arguments
    fs_group = parser.add_argument_group('Filesystem Control')
    fs_group.add_argument("--ls", action='store_true', help="List files on the device filesystem.")
    fs_group.add_argument("--format", action='store_true', help="DANGEROUS: Format the device filesystem.")
    fs_group.add_argument("--play", type=str, help="Display a static (.raw) or animated (.araw) image file.")

    args = parser.parse_args()

    # If no arguments are provided, print help and exit
    if all(v is None or v is False for v in vars(args).values()):
        parser.print_help()
        print("\nAvailable Animation IDs:")
        print("  0: Solid,  1: Breathing,  2: Cycle L-R,  3: Cycle U-D,  4: Band Sat L-R")
        print("  5: Band Sat U-D, 6: Hue Breathing, 7: Rainbow Vortex, 8: Vortex, 9: Comet Tail")
        sys.exit(1)

    device_path = find_device()
    if not device_path:
        print("Error: Keyboard not found. Is it connected?", file=sys.stderr)
        sys.exit(1)

    print("Connected to keyboard.")

    # --- Filesystem Commands ---
    if args.ls:
        list_files(device_path)
        time.sleep(0.05)

    if args.format:
        confirm = input("WARNING: This will erase all files on the device. Type 'FORMAT' to confirm: ")
        if confirm == 'FORMAT':
            print("Formatting filesystem...")
            if send_command(device_path, ID_FORMAT_FS):
                print("Format command sent. The device will take a moment to format.")
                # The device may disconnect and reconnect after format, so we wait.
                time.sleep(3)
            else:
                sys.exit(1)
        else:
            print("Format cancelled.")
        time.sleep(0.05)

    if args.play:
        print(f"Requesting to play file: {args.play}")
        payload = list(bytearray(args.play, 'utf-8'))
        if not send_command(device_path, ID_PLAY_IMAGE, payload):
            sys.exit(1)
        time.sleep(0.05)

    # --- Lighting Commands ---
    if args.rgb:
        try:
            r, g, b = map(int, args.rgb.split(','))
            if not all(0 <= val <= 255 for val in [r, g, b]):
                raise ValueError("RGB values must be between 0 and 255.")

            h, s, v = rgb_to_hsv(r, g, b)
            print(f"Setting color from RGB({r},{g},{b}) -> HSV({h},{s},{v})")

            # Send Color (HS) command
            print(f"  - Sending color (HS): {h}, {s}")
            if not send_command(device_path, ID_SET_COLOR_HS, [h, s]):
                 sys.exit(1)
            time.sleep(0.05) # Small delay between commands

            # Send Brightness (V) command
            print(f"  - Sending brightness (V): {v}")
            if not send_command(device_path, ID_SET_BRIGHTNESS, [v]):
                 sys.exit(1)
            time.sleep(0.05)

        except ValueError as e:
            print(f"Error: Invalid RGB format. {e}", file=sys.stderr)
            sys.exit(1)

    if args.brightness is not None:
        try:
            if not (0 <= args.brightness <= 255):
                raise ValueError("Brightness must be between 0 and 255.")

            print(f"Setting brightness to {args.brightness}")
            if not send_command(device_path, ID_SET_BRIGHTNESS, [args.brightness]):
                sys.exit(1)
            time.sleep(0.05)

        except ValueError as e:
            print(f"Error: Invalid brightness value. {e}", file=sys.stderr)
            sys.exit(1)

    if args.speed is not None:
        try:
            if not (0 <= args.speed <= 255):
                raise ValueError("Speed must be between 0 and 255.")

            print(f"Setting speed to {args.speed}")
            if not send_command(device_path, ID_SET_SPEED, [args.speed]):
                sys.exit(1)
            time.sleep(0.05)

        except ValueError as e:
            print(f"Error: Invalid speed value. {e}", file=sys.stderr)
            sys.exit(1)

    if args.anim is not None:
        try:
            if not (0 <= args.anim <= 9):
                raise ValueError("Animation ID must be between 0 and 9.")

            print(f"Setting animation to ID {args.anim}")
            if not send_command(device_path, ID_SET_ANIMATION, [args.anim]):
                sys.exit(1)

        except ValueError as e:
            print(f"Error: Invalid animation ID. {e}", file=sys.stderr)
            sys.exit(1)

    print("\nAll commands sent successfully.")
