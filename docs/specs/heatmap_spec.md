# ZMK Heatmap Generator - Architecture & Specification

## Executive Summary
This document outlines the design and implementation decisions for the **ZMK Heatmap Generator**, an event-driven keypress tracking and visualization system for ZMK firmware keyboards (specifically the Eyelash Corne).

Inspired by QMK heatmap tools, this solution leverages modern ZMK position event listeners, USB CDC ACM logging, and native integration with **Keymap Drawer** to automatically generate vector SVG and interactive HTML heatmaps without requiring manual key-to-matrix coordinate mapping.

---

## Key Architecture & Technology Decisions

### 1. ZMK Firmware Heatmap Module (`zmk-heatmap`)
- **Framework**: Zephyr RTOS / ZMK Event Bus.
- **Event Hook**: Subscribes to `zmk_position_state_changed` (`<zmk/events/position_state_changed.h>`).
- **Active Layer Tracking**: Fetches active layer via `zmk_keymap_highest_layer_active()`.
- **Output Protocol**: USB CDC ACM serial logging (`LOG_INF` / `printk`).
- **Data Format**: `HM:<position>,<layer>,<timestamp_ms>`
  - `position`: 0-indexed physical matrix position ID.
  - `layer`: Active layer index at press time.
  - `timestamp_ms`: Board uptime in milliseconds.
- **Reusability**: Packaged as a standard ZMK module under `modules/zmk-heatmap` with `Kconfig` and `CMakeLists.txt`. Enabled via `CONFIG_ZMK_HEATMAP=y`.

### 2. Live Data Collection CLI (`tools/zmk_heatmap_collect.py`)
- **Transport**: Listens to USB serial/CDC ACM (`/dev/tty.usbmodem*` or specified COM port) using Python `pyserial`.
- **Log File Ingestion**: Supports parsing existing text/serial logs captured via `hid_listen`, `cat`, or terminal consoles.
- **Filtering**: Extracts valid `HM:<pos>,<layer>,<timestamp>` records using regular expressions.
- **Output Dataset**: Saves normalized records into `keylog.csv` (`position,layer,timestamp`).

### 3. Keymap Drawer Integration & Heatmap Generator (`tools/zmk_heatmap_generate.py`)
- **Layout Foundation**: Utilizes Keymap Drawer SVG outputs (`eyelash_corne.svg`).
- **Automatic Matrix Mapping**: Keymap Drawer automatically assigns `class="key keypos-{N}"` to each key element in the SVG corresponding to ZMK key position `N`.
- **Visualization Engine**:
  - Calculates total press count and per-layer press distributions per key position.
  - Applies a heat color gradient (Dark Base -> Cool Cyan -> Emerald Green -> Bright Amber -> Hot Red).
  - Injects press count badges directly into the SVG key elements.
  - Generates standalone `heatmap.svg` and interactive `heatmap.html` with layer switching and usage analytics.

---

## System Flow Diagram

```
+------------------------+
| ZMK Keyboard Firmware  |
| (zmk-heatmap module)   |
+-----------+------------+
            |
            | USB CDC ACM Serial Stream ("HM:pos,layer,time")
            v
+------------------------+
| zmk_heatmap_collect.py | ---> Saves to keylog.csv
+-----------+------------+
            |
            | Keylog data + keymap-drawer SVG
            v
+------------------------+
| zmk_heatmap_generate.py| ---> Outputs heatmap.svg & heatmap.html
+------------------------+
```

---

## Detailed Component Specifications

### ZMK Module Configuration (`modules/zmk-heatmap/`)
- `Kconfig`:
  - `CONFIG_ZMK_HEATMAP`: Boolean (default `n`).
  - `CONFIG_ZMK_HEATMAP_LOG_LEVEL`: Log level selection.
- `CMakeLists.txt`:
  - Links `src/zmk_heatmap.c` into ZMK target app when `CONFIG_ZMK_HEATMAP=y`.

### Data Collection Specification
- CSV Format:
```csv
position,layer,timestamp
14,0,10450
22,0,10820
3,1,11200
```

### Heatmap Color Map
- `0%` (Unpressed): `#16171a` (Default dark keycap fill)
- `1% - 25%`: `#00e5ff` (Cool Cyan)
- `26% - 50%`: `#00e676` (Emerald Green)
- `51% - 75%`: `#ffea00` (Vibrant Yellow)
- `76% - 100%`: `#ff3d00` (Hot Red)
