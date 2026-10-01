#!/usr/bin/env python3
"""Photograph every raw LED index in three-colour batches on one PCB face."""

import argparse
import json
from pathlib import Path
import subprocess
import sys

import hid

from led_discovery import COLORS, show_leds


def scan_leds(side, led_count, camera, capture_tool, output, orientation):
    if not capture_tool.is_file():
        raise ValueError("Provide the compiled module_camera capture tool.")
    if not 1 <= led_count <= 255:
        raise ValueError("Provide a raw LED count from 1 to 255.")
    actual_count = show_leds([255, 255, 255])
    if actual_count != led_count:
        raise ValueError(f"Firmware reports {actual_count} addresses; requested {led_count}.")
    output.mkdir(parents=True, exist_ok=False)
    manifest = {
        "side": side, "led_count": led_count, "camera_id": camera,
        "orientation": orientation, "baseline": "off.png", "batches": [], "complete": False,
    }
    manifest_path = output / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
    try:
        subprocess.run([str(capture_tool), camera, str(output / "off.png")], check=True, timeout=90)
        for start in range(0, led_count, 3):
            indices = list(range(start, min(start + 3, led_count)))
            indices += [255] * (3 - len(indices))
            show_leds(indices)
            filename = f"{side}_{start:03d}.png"
            subprocess.run([str(capture_tool), camera, str(output / filename)], check=True, timeout=30)
            manifest["batches"].append({"image": filename, "indices": dict(zip(COLORS, indices))})
            manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
            print(f"Captured {side}: {min(start + 3, led_count)}/{led_count} indices", flush=True)
        manifest["complete"] = True
        manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
    finally:
        show_leds([255, 255, 255])
    print(f"Scan complete: {manifest_path}", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--side", required=True, choices=("front", "back"))
    parser.add_argument("--led-count", required=True, type=int)
    parser.add_argument("--camera", required=True, help="Explicit camera ID from module_camera --list.")
    parser.add_argument("--capture-tool", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path, help="A new directory for this face's images and manifest.")
    parser.add_argument("--orientation", required=True, help="Describe the PCB orientation in the camera image.")
    args = parser.parse_args()
    scan_leds(args.side, args.led_count, args.camera, args.capture_tool.resolve(), args.output.resolve(), args.orientation)


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, hid.HIDException, subprocess.SubprocessError) as error:
        print(f"Module scan: {error}", file=sys.stderr)
        sys.exit(1)
