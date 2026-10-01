"""Host-only updater tests: no USB or keyboard operations are performed."""

import hashlib
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
from unittest.mock import Mock, patch

import flash_module as flash


class FlashTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.firmware = self.root / "test.uf2"
        block = bytearray(512)
        struct.pack_into("<8I", block, 0, 0x0A324655, 0x9E5D5157, 0x2000,
                         0x10000000, 256, 0, 1, 0xE48BFF56)
        struct.pack_into("<I", block, 508, 0x0AB16F30)
        self.firmware.write_bytes(block)
        self.drive = Path("/Volumes/RPI-RP2")
        self.settings = {"layers": 1, "keymap": bytes(144).hex(),
                         "macros": "0000", "layout": "00000000"}

    def test_uf2_validation(self):
        self.assertEqual(flash.validate_uf2(self.firmware),
                         hashlib.sha256(self.firmware.read_bytes()).hexdigest())
        for offset, value in ((0, 0), (8, 0), (12, 0), (16, 128),
                              (20, 1), (24, 2), (28, 0), (508, 0)):
            with self.subTest(offset=offset):
                damaged = bytearray(self.firmware.read_bytes())
                struct.pack_into("<I", damaged, offset, value)
                invalid = self.root / "invalid.uf2"
                invalid.write_bytes(damaged)
                with self.assertRaises(ValueError):
                    flash.validate_uf2(invalid)
        self.firmware.write_bytes(b"short")
        with self.assertRaises(ValueError):
            flash.validate_uf2(self.firmware)

    @patch.object(flash.os, "killpg")
    @patch.object(flash.subprocess, "Popen")
    def test_stuck_child_has_no_unbounded_wait(self, popen, kill):
        child = popen.return_value
        child.communicate.side_effect = subprocess.TimeoutExpired("cat", 5)
        with self.assertRaises(TimeoutError):
            flash.run_bounded(["/bin/cat", "file"], 5)
        self.assertEqual([call.kwargs for call in child.communicate.call_args_list],
                         [{"timeout": 5}, {"timeout": 1}])
        child.wait.assert_not_called()
        child.stdout.close.assert_called_once()
        kill.assert_called_once()

    @patch.object(flash.usb.core, "find")
    def test_bootloader_requires_unique_expected_serial(self, find):
        device = Mock(serial_number="EXPECTED")
        for devices in ([], [Mock(serial_number="WRONG")], [device, device]):
            find.return_value = devices
            with self.assertRaises(ValueError):
                flash.bootloader("EXPECTED")
        find.return_value = [device]
        self.assertIs(flash.bootloader("EXPECTED"), device)

    @patch.object(flash.time, "sleep")
    @patch.object(flash, "run_bounded")
    def test_wait_for_mount_and_validate_board(self, run, sleep):
        run.side_effect = ["", "/dev/disk9 on /Volumes/RPI-RP2 (msdos, local)",
                           "UF2 Bootloader v3.0\nBoard-ID: RPI-RP2\n"]
        flash.inspect_drive(self.drive)
        self.assertEqual(run.call_args.args[0], ["/bin/cat", "/Volumes/RPI-RP2/INFO_UF2.TXT"])
        run.side_effect = ["/dev/disk9 on /Volumes/RPI-RP2 (msdos, local)", "Board-ID: WRONG"]
        with self.assertRaises(ValueError):
            flash.inspect_drive(self.drive)

    @patch.object(flash.time, "sleep")
    @patch.object(flash.usb.util, "dispose_resources")
    @patch.object(flash, "inspect_drive")
    @patch.object(flash, "bootloader")
    def test_only_read_timeout_resets_selected_device_once(self, boot, inspect, dispose, sleep):
        inspect.side_effect = [TimeoutError("stalled"), None]
        self.assertTrue(flash.prepare_drive(self.drive, "EXPECTED"))
        boot.return_value.reset.assert_called_once()
        self.assertTrue(all(call.args == ("EXPECTED",) for call in boot.call_args_list))
        boot.return_value.reset.reset_mock()
        inspect.side_effect = ValueError("wrong board")
        with self.assertRaises(ValueError):
            flash.prepare_drive(self.drive, "EXPECTED")
        boot.return_value.reset.assert_not_called()
        inspect.side_effect = [TimeoutError("first"), TimeoutError("again")]
        with self.assertRaises(TimeoutError):
            flash.prepare_drive(self.drive, "EXPECTED")
        boot.return_value.reset.assert_called_once()

    @patch.object(flash.time, "sleep")
    @patch.object(flash.time, "monotonic", side_effect=[0, 0, 21])
    @patch.object(flash.hid, "enumerate")
    def test_raw_hid_without_keyboard_is_not_success(self, enumerate_hid, monotonic, sleep):
        enumerate_hid.return_value = [{"usage_page": flash.RAW_USAGE_PAGE, "usage": flash.RAW_USAGE}]
        with self.assertRaisesRegex(ValueError, "both Module"):
            flash.wait_for_module()

    @patch.object(flash.time, "sleep")
    @patch.object(flash.usb.core, "find", return_value=[])
    @patch.object(flash, "via_command")
    def test_stability_rejects_reboot(self, command, find, sleep):
        command.side_effect = [b"\x01\x00\x0c", b"\x02\x01" + (2000).to_bytes(4, "big"),
                               b"\x02\x01" + (1).to_bytes(4, "big")]
        with self.assertRaisesRegex(ValueError, "rebooted"):
            flash.verify_stability(Mock())

    @patch.object(flash.usb.core, "find", return_value=[Mock()])
    @patch.object(flash, "via_command", return_value=b"\x01\x00\x0c")
    def test_stability_rejects_bootloader(self, command, find):
        with self.assertRaisesRegex(ValueError, "bootloader"):
            flash.verify_stability(Mock())

    @patch.object(flash.sys, "platform", "darwin")
    @patch.object(flash.usb.core, "find", return_value=[])
    @patch.object(flash, "find_module", return_value={"path": b"module"})
    @patch.object(flash.hid, "Device")
    @patch.object(flash, "read_settings")
    @patch.object(flash, "run_bounded")
    def test_check_never_reboots_copies_or_archives(self, run, read, device, find_module, find_usb):
        read.return_value = self.settings
        recovery = self.root / "recovery"
        flash.flash_module(self.firmware, self.drive, "EXPECTED", recovery, "application", None, True)
        self.assertFalse(recovery.exists())
        device.return_value.__enter__.return_value.write.assert_not_called()
        run.assert_not_called()

    @patch.object(flash.sys, "platform", "darwin")
    @patch.object(flash, "bootloader")
    @patch.object(flash, "prepare_drive", return_value=False)
    @patch.object(flash, "wait_for_module", return_value={"path": b"module"})
    @patch.object(flash, "restore_settings")
    @patch.object(flash, "verify_stability")
    @patch.object(flash.hid, "Device")
    @patch.object(flash, "run_bounded")
    def test_copy_targets_one_drive_and_preserves_image(self, run, device, verify, restore, wait, prepare, boot):
        snapshot = self.root / "settings.json"
        snapshot.write_text(json.dumps(self.settings))
        recovery = self.root / "recovery"
        flash.flash_module(self.firmware, self.drive, "EXPECTED", recovery, "bootloader", snapshot, False)
        archive = next(recovery.glob("*/*.uf2"))
        self.assertEqual(archive.read_bytes(), self.firmware.read_bytes())
        self.assertEqual(archive.stat().st_mode & 0o777, 0o444)
        run.assert_called_once_with(["/bin/cp", "-X", str(archive), "/Volumes/RPI-RP2/MODULE.UF2"], 30)
        report = json.loads(next(recovery.glob("*/flash-report.json")).read_text())
        self.assertEqual(report["stage"], "usb_verified")
        self.assertEqual(report["human_typing_and_lighting_confirmation"], "pending")
        restore.assert_called_once()
        verify.assert_called_once()

        # A failed copy is recorded, never retried or mistaken for verification.
        completed_run = archive.parent
        run.reset_mock()
        verify.reset_mock()
        restore.reset_mock()
        run.side_effect = TimeoutError("copy stalled")
        with self.assertRaisesRegex(TimeoutError, "copy stalled"):
            flash.flash_module(self.firmware, self.drive, "EXPECTED", recovery, "bootloader", snapshot, False)
        run.assert_called_once()
        verify.assert_not_called()
        restore.assert_not_called()
        failed_run = next(path for path in recovery.iterdir() if path != completed_run)
        failed_report = json.loads((failed_run / "flash-report.json").read_text())
        self.assertEqual(failed_report["stage"], "copying_uf2")
        self.assertEqual(failed_report["error"], "copy stalled")
        self.assertEqual(next(failed_run.glob("*.uf2")).read_bytes(), self.firmware.read_bytes())


if __name__ == "__main__":
    unittest.main()
