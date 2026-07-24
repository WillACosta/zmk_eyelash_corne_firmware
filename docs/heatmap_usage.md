# ZMK Heatmap Module - Usage & Integration Guide

This guide explains how to enable, collect data from, and generate keypress heatmaps using the **ZMK Heatmap** module and tooling integrated into this repository.

---

## 1. Firmware Setup & Configuration

### Enabling in this Repository (Eyelash Corne)
The module is already included in `zephyr/module.yml` and enabled in `config/eyelash_corne.conf`:

```properties
# === Heatmap Generator ===
CONFIG_ZMK_HEATMAP=y
CONFIG_SERIAL=y
CONFIG_ZMK_USB_LOGGING=y
```



When enabled, your keyboard logs keypress events over the USB serial console (CDC ACM) in real time whenever a key is pressed:
`HM:<position>,<layer>,<timestamp_ms>`

### Sharing & Integrating into Other ZMK Keyboards
To use this module on any other ZMK keyboard repository:

1. Add the module to your `config/west.yml`:
   ```yaml
   manifest:
     projects:
       - name: zmk-heatmap
         url: https://github.com/WillACosta/zmk_eyelash_corne_firmware
         revision: feat/heat-map
         path: modules/zmk-heatmap
   ```

2. Or copy the `modules/zmk-heatmap` folder into your repo and update your `zephyr/module.yml`:
   ```yaml
   name: zmk-config
   build:
     cmake: modules/zmk-heatmap
     kconfig: modules/zmk-heatmap/Kconfig
     settings:
       board_root: .
   ```

3. Add `CONFIG_ZMK_HEATMAP=y` to your keyboard's `.conf` file and build your firmware using GitHub Actions or `west build`.

---

## 2. Collecting Live Keypress Data

The firmware outputs key press events over the serial console. Use the provided Python collection tool `tools/zmk_heatmap_collect.py`.

### Option A: Live USB Serial Monitoring (Recommended)
Plug in your keyboard via USB and run:

```bash
# Auto-detects ZMK serial port (/dev/tty.usbmodem* or COM*)
python3 tools/zmk_heatmap_collect.py

# Or specify a custom port / baud rate:
python3 tools/zmk_heatmap_collect.py --port /dev/tty.usbmodem14101 --output my_keylog.csv
```

As you type, the tool records every key press event and appends it to `keylog.csv`. Press `Ctrl+C` to end session.

### Option B: Parsing Captured Log Files
If you captured serial logs using `hid_listen`, `cat /dev/tty.usbmodem* > raw.log`, `screen`, or a terminal emulator:

```bash
python3 tools/zmk_heatmap_collect.py --input raw.log --output keylog.csv
```

### Output File (`keylog.csv`)
```csv
position,layer,timestamp
14,0,10450
22,0,10820
3,1,11200
```

---

## 3. Generating Visual Heatmaps

The heatmap generator tool (`tools/zmk_heatmap_generate.py`) parses your `keylog.csv` data and overlays key press frequencies onto vector SVG layouts produced by **Keymap Drawer**.

### Running the Generator

```bash
python3 tools/zmk_heatmap_generate.py \
  --keylog keylog.csv \
  --svg keymap-drawer/eyelash_corne.svg \
  --output-svg heatmap.svg \
  --output-html heatmap.html
```

### Output Files
1. **`heatmap.svg`**: Standalone vector SVG keymap styled with heat intensity color fills and press count badges on each key.
2. **`heatmap.html`**: Self-contained interactive web report featuring:
   - Summary statistics (Total Presses, Unique Keys Used, Top Active Key).
   - Dynamic SVG Heatmap display.
   - Keypress distribution analytics table sorted by frequency.

---

## 4. Keymap Drawer Integration Details

The generator leverages Keymap Drawer's native class naming scheme:
- Keymap Drawer tags every key shape in the SVG output with `class="key keypos-{N}"`, where `{N}` corresponds exactly to ZMK's 0-indexed matrix position `N`.
- The generator updates key shape fills and glows using a smooth color ramp:
  - **Unpressed / Base**: `#16171a`
  - **1% – 25% (Cool)**: `#00e5ff` (Cyan)
  - **26% – 50% (Medium)**: `#00e676` (Emerald Green)
  - **51% – 75% (High)**: `#ffea00` (Amber Yellow)
  - **76% – 100% (Hot)**: `#ff3d00` (Hot Red)
