import sys
import hid
import struct

# --- Device Identification ---
VID = 0x1067
PID = 0x626D
USAGE_PAGE = 0xFF60
USAGE = 0x61
PACKET_SIZE = 32

# --- Custom Command ID (must match firmware) ---
ID_SET_LED_RED = 0x70

def main():
    """
    Sends a command via Raw HID to turn a specific LED on the keyboard RED.
    """
    # 1. Check command-line arguments
    if len(sys.argv) != 2:
        print(f"Usage: python3 {sys.argv[0]} <led_index>")
        print("       <led_index> is an integer from 0 to 89.")
        sys.exit(1)

    try:
        led_index = int(sys.argv[1])
        if not (0 <= led_index <= 89):
            raise ValueError("LED index must be between 0 and 89.")
    except ValueError as e:
        print(f"Error: Invalid LED index. {e}")
        sys.exit(1)

    # 2. Find the HID device
    device_path = None
    try:
        for dev in hid.enumerate(VID, PID):
            if dev.get('usage_page') == USAGE_PAGE and dev.get('usage') == USAGE:
                device_path = dev['path']
                print(f"Found device: {dev['manufacturer_string']} {dev['product_string']}")
                break
    except hid.HIDException as e:
        print(f"Error enumerating HID devices: {e}")
        print("This may be a permissions issue. On Linux, try running with 'sudo' or setting up udev rules.")
        sys.exit(1)


    if not device_path:
        print("Error: Could not find matching HID device.")
        print(f"Ensure device with VID={VID:04X}, PID={PID:04X}, UsagePage={USAGE_PAGE:04X}, Usage={USAGE:02X} is connected.")
        sys.exit(1)

    try:
        # 3. Open the device
        device = hid.Device(path=device_path)
        print(f"Opened device. Sending command to activate LED {led_index}.")

        # 4. Construct the packet
        # Format: Magic (1B), Command (1B), PacketID (4B), Payload...
        packet_id = 0  # Can be static for this simple script

        # Header (6 bytes): Magic, Command ID, Packet ID (little-endian)
        header = struct.pack('<BBI', 0x09, ID_SET_LED_RED, packet_id)

        # Payload (1 byte): The LED index
        payload = struct.pack('<B', led_index)

        # Combine and pad to the required packet size
        packet = (header + payload).ljust(PACKET_SIZE, b'\x00')

        print(f"Sending packet: {packet.hex()}")
        device.write(packet)

        # 5. Close the device
        device.close()
        print("Command sent successfully.")

    except hid.HIDException as e:
        print(f"Error communicating with HID device: {e}")
        print("Please ensure no other program (like QMK Toolbox) is connected to the HID interface.")
        sys.exit(1)
    except Exception as e:
        print(f"An unexpected error occurred: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
