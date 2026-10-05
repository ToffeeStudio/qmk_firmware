"""Offline rescue tests: no device enumeration, USB writes or firmware changes."""

from collections import deque
import io
import struct
import unittest
from unittest.mock import Mock, patch

import rescue_lighting as rescue


def reply(packet_id, status=0, command=0x7C, payload=b""):
    data = bytearray(32)
    data[0] = status
    data[1:1 + len(payload)] = payload
    data[26:28] = bytes((0xA5, command))
    data[28:32] = struct.pack("<I", packet_id)
    return bytes(data)


class RescueTests(unittest.TestCase):
    def test_packet_is_32_bytes_plus_report_id_and_all_five_slots_off(self):
        packet = rescue.off_report(0x12345678)
        self.assertEqual(len(packet), 33)
        self.assertEqual(packet[:7], b"\x00\x09\x7c\x78\x56\x34\x12")
        self.assertEqual(packet[7:12], b"\xff" * 5)
        self.assertEqual(packet[12:], b"\x00" * 21)

    def test_find_uses_module_identity_and_raw_interface(self):
        with patch.object(rescue.hid, "enumerate", return_value=[
            {"path": b"keyboard", "usage_page": 1, "usage": 6},
            {"path": b"raw", "usage_page": 0xFF60, "usage": 0x61},
        ]) as enumerate_hid:
            self.assertEqual(rescue.module_path(), b"raw")
            enumerate_hid.assert_called_once_with(0x1067, 0x626D)

    def test_absent_board_is_expected_while_armed(self):
        with patch.object(rescue.hid, "enumerate", return_value=[]):
            self.assertIsNone(rescue.module_path())

    def test_two_boards_fail_before_open(self):
        info = {"path": b"raw", "usage_page": 0xFF60, "usage": 0x61}
        with patch.object(rescue.hid, "enumerate", return_value=[info, info]), patch.object(rescue.hid, "Device") as open_hid:
            with self.assertRaisesRegex(ValueError, "only one"):
                rescue.module_path()
            open_hid.assert_not_called()

    def test_ack_requires_matching_tag_and_packet_id(self):
        device = Mock()
        device.read.side_effect = [b"\x00", bytes(32), reply(99), reply(7), b""]
        pending = deque([7])
        self.assertEqual(rescue.receive_ack(device, pending, deque()), (True, False, False))
        self.assertEqual(list(pending), [])
        self.assertTrue(all(call.kwargs == {"timeout": 0} for call in device.read.call_args_list))

    def test_rejection_is_not_success(self):
        device = Mock()
        device.read.return_value = reply(7, 0xEF)
        with self.assertRaisesRegex(ValueError, "rejected"):
            rescue.receive_ack(device, deque([7]), deque())

    def test_read_drain_is_bounded_and_no_reply_is_not_success(self):
        device = Mock()
        device.read.return_value = bytes(32)
        self.assertEqual(rescue.receive_ack(device, deque([7]), deque()), (False, False, False))
        self.assertEqual(device.read.call_count, 32)
        device.read.return_value = b""
        self.assertEqual(rescue.receive_ack(device, deque([7]), deque()), (False, False, False))

    def run_simulation(self, paths, devices, write_cost=0):
        now = [0.0]
        sleeps = []
        writes = []
        def write(data):
            writes.append((now[0], data))
            now[0] += write_cost
            return len(data)
        def sleep(duration):
            sleeps.append(duration)
            now[0] += duration
        for device in devices:
            device.write.side_effect = write
            device.read.return_value = b""
        output = io.StringIO()
        with patch.object(rescue, "module_path", side_effect=[*paths, KeyboardInterrupt]), \
             patch.object(rescue.hid, "Device", side_effect=devices) as opened, \
             patch.object(rescue.time, "monotonic", side_effect=lambda: now[0]), \
             patch.object(rescue.time, "sleep", side_effect=sleep), \
             patch("sys.stdout", output):
            rescue.rescue()
        return writes, sleeps, opened, output.getvalue()

    def test_hotplug_sends_immediately_then_every_200ms(self):
        device = Mock()
        writes, sleeps, opened, output = self.run_simulation([None, b"a", b"a", b"a"], [device], 0.025)
        self.assertEqual(len(writes), 3)
        for (stamp, packet), expected in zip(writes, [0.2, 0.4, 0.6]):
            self.assertAlmostEqual(stamp, expected)
            self.assertEqual(packet[1:3], b"\x09\x7c")
        self.assertAlmostEqual(sleeps[1], 0.175)
        opened.assert_called_once_with(path=b"a")
        device.close.assert_called_once()
        self.assertIn("Recovery NOT confirmed", output)
        self.assertIn("Permanent reset NOT confirmed", output)
        self.assertEqual(device.method_calls[0][0], "write")

    def test_reconnect_resolves_new_path(self):
        first, second = Mock(), Mock()
        writes, _, opened, _ = self.run_simulation([b"a", None, b"b"], [first, second])
        self.assertEqual([call.kwargs for call in opened.call_args_list], [{"path": b"a"}, {"path": b"b"}])
        self.assertEqual(len(writes), 2)
        first.close.assert_called_once()
        second.close.assert_called_once()

    def test_usb_failure_retries_and_closes_handle(self):
        device = Mock()
        device.write.side_effect = [rescue.hid.HIDException("disconnected"), 33]
        device.read.return_value = b""
        with patch.object(rescue, "module_path", side_effect=[b"a", b"a", KeyboardInterrupt]), \
             patch.object(rescue.hid, "Device", return_value=device) as opened, \
             patch.object(rescue.time, "sleep"), patch("sys.stdout", new_callable=io.StringIO) as output:
            rescue.rescue()
        self.assertEqual(opened.call_count, 2)
        self.assertEqual(device.close.call_count, 2)
        self.assertIn("USB unavailable", output.getvalue())

    def test_short_write_is_not_success(self):
        device = Mock()
        device.write.return_value = 12
        with patch.object(rescue, "module_path", side_effect=[b"a", KeyboardInterrupt]), \
             patch.object(rescue.hid, "Device", return_value=device), \
             patch.object(rescue.time, "sleep"), patch("sys.stdout", new_callable=io.StringIO) as output:
            rescue.rescue()
        device.read.assert_not_called()
        self.assertIn("Short HID write", output.getvalue())

    def test_dry_run_never_enumerates_or_opens_hid(self):
        with patch("sys.argv", ["rescue_lighting.py", "--dry-run"]), \
             patch.object(rescue.hid, "enumerate") as enumerate_hid, \
             patch.object(rescue.hid, "Device") as open_hid, patch("sys.stdout", new_callable=io.StringIO):
            rescue.main()
        enumerate_hid.assert_not_called()
        open_hid.assert_not_called()

    def test_save_packet_has_explicit_zero_guard(self):
        packet = rescue.save_off_report(17)
        self.assertEqual(len(packet), 33)
        self.assertEqual(packet[:11], b"\x00\x09\x8b\x11\x00\x00\x00ZERO")
        self.assertEqual(packet[11:], bytes(22))

    def test_save_ack_requires_verified_schema_and_matching_id(self):
        device = Mock()
        device.read.side_effect = [reply(8, command=0x8B, payload=b"\x01\x07\x00\x00\x00"), b""]
        self.assertEqual(rescue.receive_ack(device, deque(), deque([7])), (False, False, False))
        device.read.side_effect = [reply(7, command=0x8B, payload=b"\x01\x07\x00\x00\x00"), b""]
        self.assertEqual(rescue.receive_ack(device, deque(), deque([7])), (False, True, False))

    def test_unsupported_is_explicitly_not_permanent_success(self):
        device = Mock()
        device.read.side_effect = [reply(7, status=0xEF, command=0x8B), b""]
        self.assertEqual(rescue.receive_ack(device, deque(), deque([7])), (False, False, True))

    def test_failed_or_malformed_storage_ack_fails_closed(self):
        for status, payload in [(0xEB, b""), (0, b""), (0, b"\x01\x03\x00\x00\x00"), (0, b"\x01\x07\x00\x01\x00")]:
            with self.subTest(status=status, payload=payload):
                device = Mock(); device.read.return_value = reply(7, status, 0x8B, payload)
                with self.assertRaisesRegex(ValueError, "NOT verified"):
                    rescue.receive_ack(device, deque(), deque([7]))

    def test_live_sequence_blanks_before_save_and_does_not_repeat_saved_write(self):
        device = Mock(); replies = []; commands = []
        def write(data):
            command = data[2]; packet_id = int.from_bytes(data[3:7], "little")
            commands.append(command)
            payload = b"\x01\x07\x00\x00\x00" if command == 0x8B else b""
            replies.append(reply(packet_id, command=command, payload=payload))
            return len(data)
        device.write.side_effect = write
        device.read.side_effect = lambda *args, **kwargs: replies.pop(0) if replies else b""
        with patch.object(rescue, "module_path", side_effect=[b"a"] * 4 + [KeyboardInterrupt]), \
             patch.object(rescue.hid, "Device", return_value=device), \
             patch.object(rescue.time, "sleep"), patch("sys.stdout", new_callable=io.StringIO) as output:
            rescue.rescue()
        self.assertEqual(commands, [0x7C, 0x8B, 0x7C, 0x7C, 0x7C])
        self.assertIn("SAVED OFF VERIFIED", output.getvalue())
        self.assertIn("Saved zero brightness was verified", output.getvalue())


if __name__ == "__main__":
    unittest.main()
