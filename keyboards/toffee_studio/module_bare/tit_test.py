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
MAGIC = 0x09
ID_SET_ANIMATION = 0x71
ID_SET_SPEED = 0x72
ID_SET_COLOR_HSV = 0x73

# ----------------------- FOR TESTING ONLY -----------------------------
ID_SET_LEFT_RGB = 0x74
ID_SET_RIGHT_RGB = 0x75
# ----------------------------------------------------------------------

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

def send_command(device_path, command_id, payload):
    """Sends a command with a payload to the keyboard."""
    if not device_path:
        print("Error: Keyboard not found.", file=sys.stderr)
        return False

    try:
        with hid.Device(path=device_path) as h:
            # Construct the 64-byte packet
            packet = [0x00] * 65
            packet[1] = MAGIC
            packet[2] = command_id
            # Bytes 3-6 are unused packet ID

            # Copy payload into the packet starting at byte 7
            for i, val in enumerate(payload):
                if 7 + i < len(packet):
                    packet[7 + i] = val

            # The hid library prepends the report ID, so we send 64 bytes.
            h.write(bytes(packet[1:]))
            return True

    except Exception as e:
        print(f"An error occurred: {e}", file=sys.stderr)
        return False

def rgb_to_hsv(r, g, b):
    """Converts RGB (0-255) to HSV (0-255 for QMK)"""
    # colorsys expects values between 0 and 1
    h, s, v = colorsys.rgb_to_hsv(r / 255.0, g / 255.0, b / 255.0)
    # QMK HSV uses 0-255 for all components
    return int(h * 255), int(s * 255), int(v * 255)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Control keyboard LEDs. Use --left/--right for direct control or --anim/--speed for underglow effects.")

    # --- New arguments for left/right control ---
    side_group = parser.add_argument_group('Direct LED Control', 'Control specific halves of the keyboard. Overrides animations.')
    side_group.add_argument("--left", action="store_true", help="Target the left half of the keyboard.")
    side_group.add_argument("--right", action="store_true", help="Target the right half of the keyboard.")

    # --- Existing arguments for animations ---
    anim_group = parser.add_argument_group('Animation Control', 'Control the built-in underglow animations.')
    anim_group.add_argument("--rgb", type=str, help="Set color as R,G,B (e.g., '255,0,128'). Used by both modes.")
    anim_group.add_argument("--anim", type=int, help="Set animation ID (0-9).")
    anim_group.add_argument("--speed", type=int, help="Set animation speed (0-255).")

    args = parser.parse_args()

    # If no arguments are given, print help.
    if len(sys.argv) == 1:
        parser.print_help()
        print("\nExamples:")
        print("  python3 tit_test.py --left --rgb 255,0,0")
        print("  python3 tit_test.py --right --rgb 0,0,255")
        print("  python3 tit_test.py --anim 7 --speed 150")
        print("\nAvailable Animation IDs:")
        print("  0: Solid Color,  1: Breathing,  2: Cycle Left-Right,  3: Cycle Up-Down")
        print("  4: Band Sat L-R, 5: Band Sat U-D, 6: Hue Breathing,   7: Rainbow Vortex")
        print("  8: Vortex (Sat), 9: Comet Tail")
        sys.exit(1)

    device_path = find_device()
    if not device_path:
        print("Error: Keyboard not found. Is it connected?", file=sys.stderr)
        sys.exit(1)

    print("Connected to keyboard.")

    commands_sent = 0

    # --- Handle Left/Right Direct Control ---
    if args.left or args.right:
        if not args.rgb:
            print("Error: --rgb is required when using --left or --right.", file=sys.stderr)
            sys.exit(1)
        if args.left and args.right:
            print("Error: Please choose --left OR --right, not both.", file=sys.stderr)
            sys.exit(1)

        try:
            r, g, b = map(int, args.rgb.split(','))
            if not all(0 <= val <= 255 for val in [r, g, b]):
                raise ValueError("RGB values must be between 0 and 255.")

            command_id = ID_SET_LEFT_RGB if args.left else ID_SET_RIGHT_RGB
            side = "left" if args.left else "right"

            print(f"Setting {side} half to RGB({r},{g},{b})")
            if send_command(device_path, command_id, [r, g, b]):
                commands_sent += 1
            else:
                sys.exit(1) # Exit if command fails

        except ValueError as e:
            print(f"Error: Invalid RGB format. {e}", file=sys.stderr)
            sys.exit(1)

    # --- Handle Animation Control ---
    # This block can run alongside the side-control if other args are passed
    if args.rgb and not (args.left or args.right):
        try:
            r, g, b = map(int, args.rgb.split(','))
            if not all(0 <= val <= 255 for val in [r, g, b]):
                raise ValueError("RGB values must be between 0 and 255.")

            h, s, v = rgb_to_hsv(r, g, b)
            print(f"Setting animation color to RGB({r},{g},{b}) -> HSV({h},{s},{v})")
            if send_command(device_path, ID_SET_COLOR_HSV, [h, s, v]):
                 commands_sent += 1
                 time.sleep(0.05) # Small delay between commands
            else:
                 sys.exit(1)

        except ValueError as e:
            print(f"Error: Invalid RGB format. {e}", file=sys.stderr)
            sys.exit(1)

    if args.speed is not None:
        try:
            if not (0 <= args.speed <= 255):
                raise ValueError("Speed must be between 0 and 255.")

            print(f"Setting speed to {args.speed}")
            if send_command(device_path, ID_SET_SPEED, [args.speed]):
                commands_sent += 1
                time.sleep(0.05)
            else:
                sys.exit(1)

        except ValueError as e:
            print(f"Error: Invalid speed value. {e}", file=sys.stderr)
            sys.exit(1)

    if args.anim is not None:
        try:
            if not (0 <= args.anim <= 9):
                raise ValueError("Animation ID must be between 0 and 9.")

            print(f"Setting animation to ID {args.anim}")
            if send_command(device_path, ID_SET_ANIMATION, [args.anim]):
                commands_sent += 1
            else:
                sys.exit(1)

        except ValueError as e:
            print(f"Error: Invalid animation ID. {e}", file=sys.stderr)
            sys.exit(1)

    if commands_sent > 0:
        print("All commands sent successfully.")
    else:
        print("No valid commands were specified. Use -h for help.")
