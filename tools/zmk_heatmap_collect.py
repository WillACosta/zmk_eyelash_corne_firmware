#!/usr/bin/env python3
"""
ZMK Heatmap Data Collector
Listens to live serial logging from ZMK firmware or parses raw log files,
extracting keypress events and saving them to keylog.csv.
"""

import argparse
import sys
import re
import csv
import time
from pathlib import Path

# Regular expression to match ZMK heatmap log format: HM:<position>,<layer>,<timestamp>
HM_REGEX = re.compile(r"HM:(\d+),(\d+),(\d+)")

def parse_args():
    parser = argparse.ArgumentParser(description="ZMK Heatmap Data Collector")
    parser.add_argument("--port", "-p", help="Serial port (e.g. /dev/tty.usbmodem14101 or COM3)")
    parser.add_argument("--baud", "-b", type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument("--input", "-i", help="Input log file to parse instead of live serial stream")
    parser.add_argument("--output", "-o", default="keylog.csv", help="Output CSV file (default: keylog.csv)")
    return parser.parse_args()

def process_line(line, writer, counts):
    match = HM_REGEX.search(line)
    if match:
        pos, layer, timestamp = int(match.group(1)), int(match.group(2)), int(match.group(3))
        writer.writerow([pos, layer, timestamp])
        counts[pos] = counts.get(pos, 0) + 1
        return pos, layer, timestamp
    return None

def collect_from_file(file_path, output_path):
    print(f"Reading ZMK log file: {file_path}")
    counts = {}
    total_events = 0

    with open(file_path, "r", encoding="utf-8", errors="ignore") as f_in, \
         open(output_path, "w", newline="", encoding="utf-8") as f_out:
        writer = csv.writer(f_out)
        writer.writerow(["position", "layer", "timestamp"])

        for line in f_in:
            res = process_line(line, writer, counts)
            if res:
                total_events += 1

    print(f"Processed {total_events} keypress events into {output_path}.")
    print("Summary press counts per key position:")
    for pos in sorted(counts.keys()):
        print(f"  Keypos {pos:2d}: {counts[pos]} presses")

def collect_from_serial(port, baud, output_path):
    try:
        import serial
        import serial.tools.list_ports
    except ImportError:
        print("Error: pyserial is required for live serial monitoring.")
        print("Install it with: pip install pyserial")
        sys.exit(1)

    if not port:
        ports = list(serial.tools.list_ports.comports())
        zmk_ports = [p.device for p in ports if "usbmodem" in p.device.lower() or "ttyACM" in p.device]
        if zmk_ports:
            port = zmk_ports[0]
            print(f"Auto-detected ZMK serial port: {port}")
        else:
            if ports:
                print("Available serial ports:")
                for p in ports:
                    print(f"  {p.device} - {p.description}")
                port = ports[0].device
                print(f"Using default port: {port}")
            else:
                print("Error: No serial ports found. Please connect your ZMK keyboard or specify --input file.")
                sys.exit(1)

    print(f"Connecting to ZMK board on {port} @ {baud} baud...")
    counts = {}
    total_events = 0

    # Ensure output file exists and has headers
    out_file = Path(output_path)
    file_exists = out_file.exists() and out_file.stat().st_size > 0

    try:
        ser = serial.Serial(port, baud, timeout=1)
        print("Listening for keypresses... Press Ctrl+C to stop.\n")

        with open(output_path, "a", newline="", encoding="utf-8") as f_out:
            writer = csv.writer(f_out)
            if not file_exists:
                writer.writerow(["position", "layer", "timestamp"])
                f_out.flush()

            while True:
                line = ser.readline().decode("utf-8", errors="ignore")
                if line:
                    res = process_line(line, writer, counts)
                    if res:
                        total_events += 1
                        pos, layer, ts = res
                        f_out.flush()
                        print(f"\r[Live] Keypos: {pos:2d} | Layer: {layer} | Total Recorded: {total_events}", end="", flush=True)

    except KeyboardInterrupt:
        print(f"\nStopped collection. Total events recorded in this session: {total_events}")
        print(f"Saved to {output_path}")

def main():
    args = parse_args()
    if args.input:
        collect_from_file(args.input, args.output)
    else:
        collect_from_serial(args.port, args.baud, args.output)

if __name__ == "__main__":
    main()
