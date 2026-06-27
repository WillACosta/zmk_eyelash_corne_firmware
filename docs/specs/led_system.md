# Corne ZMK LED Status System (V1)

## Overview

Implement a custom LED-based status system for a wireless Corne keyboard running ZMK.

The objective is to provide keyboard status feedback without requiring OLED displays, screens, host-side software, or permanent status LEDs.

The system will leverage existing per-key RGB LEDs to indicate:

* Active Bluetooth profile
* Bluetooth pairing state
* Host connection status
* Split connection status
* Battery level

Version 1 intentionally limits all status indication to the **left half (central side)** to simplify implementation and avoid custom split synchronization protocols.

---

# Design Principles

* No OLED or display dependency
* Low power consumption
* Status LEDs remain OFF by default
* Information is shown only when requested or when an important event occurs
* Reuse existing RGB key LEDs
* Follow ZMK event-driven architecture
* Minimize modifications to upstream ZMK components

---

# Scope

## Included

* Bluetooth profile indication
* Bluetooth pairing indication
* Host connection status
* Split connection status
* Battery level indication
* Combo-triggered status visualization

## Excluded (V1)

* Right-half LED synchronization
* Continuous battery display
* Charging indicators
* Low-battery warnings
* RGB animations unrelated to status
* Custom split transport protocols
* OLED support

---

# Hardware Assumptions

The keyboard is a wireless Corne running ZMK and equipped with individually addressable RGB LEDs (WS2812/SK6812 or equivalent).

The left half acts as the ZMK Central device.

The right half acts as the Peripheral device.

All status indications are displayed exclusively on the left half.

---

# LED Mapping

## Bluetooth Profiles

The following LEDs are reserved for Bluetooth profile indication:

| Key | Profile   |
| --- | --------- |
| A   | Profile 0 |
| S   | Profile 1 |
| D   | Profile 2 |
| F   | Profile 3 |
| G   | Profile 4 |

These LEDs shall not be reused by other status indicators.

---

## Battery Indicators

The left-most column on the left half is reserved for battery visualization.

| Key |
| --- |
| Q   |
| A   |
| Z   |

These LEDs act as a battery level bar graph.

> Note: Since `A` is also reserved for Bluetooth profile indication, the implementation should support remapping if overlap becomes problematic. The final LED indices should be configurable in a dedicated configuration file.

---

## Connection Indicators

| Key | Purpose          |
| --- | ---------------- |
| V   | Host Connection  |
| B   | Split Connection |

---

# Functional Requirements

## FR-1 Bluetooth Profile Selection

### Trigger

User switches Bluetooth profile.

### Behavior

The corresponding profile LED shall:

* Turn BLUE
* Remain illuminated for 500 ms
* Turn OFF automatically

### Example

Selecting Profile 2:

* D LED lights BLUE
* D LED turns OFF after 500 ms

---

## FR-2 Bluetooth Pairing Mode

### Trigger

User enters Bluetooth pairing mode.

### Behavior

The corresponding profile LED shall blink BLUE.

Suggested pattern:

* ON 150 ms
* OFF 150 ms
* Repeat 5 times

After completion:

* LED OFF

### Example

Entering pairing mode for Profile 1:

* S LED blinks BLUE
* Animation ends
* S LED OFF

---

## FR-3 Host Connection Status

### Trigger

Dedicated status combo.

### LED

V

### Connected

* GREEN
* 1000 ms

### Disconnected

* RED
* 1000 ms

### End State

* LED OFF

---

## FR-4 Split Connection Status

### Trigger

Dedicated status combo.

### LED

B

### Connected

* GREEN
* 1000 ms

### Disconnected

* RED
* 1000 ms

### End State

* LED OFF

---

## FR-5 Battery Status

### Trigger

Dedicated status combo.

### Duration

1000 ms

### Color Thresholds

#### Green

Battery > 80%

#### Orange

Battery > 40% and ≤ 80%

#### Red

Battery ≤ 40%

---

## Battery Bar Representation

Three LEDs represent battery level.

### 80–100%

● ● ●

### 60–79%

● ● ○

### 40–59%

● ● ○

### 20–39%

● ○ ○

### 0–19%

● ○ ○

Color is determined by the threshold group.

---

## Examples

### 92%

Q = Green
A = Green
Z = Green

### 65%

Q = Orange
A = Orange
Z = Off

### 15%

Q = Red
A = Off
Z = Off

---

# Key Combos

Initial proposal:

| Combo     | Action              |
| --------- | ------------------- |
| Lower + B | Show Battery Status |
| Lower + N | Show Host Status    |
| Lower + M | Show Split Status   |

Requirements:

* Must not interfere with normal typing
* Must be easy to access
* Should remain configurable

---

# Architecture

## Module Structure

Create a dedicated feature module:

```text
app/src/led_status/
├── led_status.c
├── led_status.h
└── led_status_config.h
```

---

## Responsibilities

### led_status.c

Responsible for:

* Event subscriptions
* Status animations
* LED updates
* Timeout handling

### led_status.h

Public API.

### led_status_config.h

Configurable LED mappings.

Example:

```c
#define LED_PROFILE_0
#define LED_PROFILE_1
#define LED_PROFILE_2
#define LED_PROFILE_3
#define LED_PROFILE_4

#define LED_BATTERY_TOP
#define LED_BATTERY_MIDDLE
#define LED_BATTERY_BOTTOM

#define LED_HOST_STATUS
#define LED_SPLIT_STATUS
```

---

# Event Integration

## Bluetooth Events

Subscribe to available ZMK Bluetooth events.

Examples:

```c
zmk_bt_profile_changed
```

```c
zmk_bt_profile_pairing_complete
```

```c
zmk_bt_profile_pairing_failed
```

Equivalent upstream events may be used if names differ.

---

## Host Connection Events

Use existing ZMK endpoint or Bluetooth connection events.

Examples:

```c
zmk_endpoint_changed
```

or equivalent APIs available in the current ZMK version.

---

## Split Connection State

Use the official ZMK split APIs.

The central half should determine whether the peripheral half is connected.

No custom transport implementation shall be introduced in V1.

---

## Battery Information

Use ZMK battery APIs.

Do not create a parallel battery monitoring subsystem.

---

# Public API

Suggested helper functions:

```c
void led_status_show_profile(uint8_t profile);
```

```c
void led_status_show_pairing(uint8_t profile);
```

```c
void led_status_show_host(bool connected);
```

```c
void led_status_show_split(bool connected);
```

```c
void led_status_show_battery(uint8_t percentage);
```

---

# Timing Requirements

## Bluetooth Profile

500 ms

## Pairing Animation

150 ms ON

150 ms OFF

5 cycles

## Status Queries

1000 ms

After timeout:

* LEDs OFF

---

# Power Requirements

To preserve battery life:

* LEDs OFF by default
* LEDs only illuminate during events or status requests
* No continuous animations
* No idle RGB effects

Battery efficiency is a primary design goal.

---

# Testing Plan

## Bluetooth Profile Selection

Verify:

* Profile 0 → A
* Profile 1 → S
* Profile 2 → D
* Profile 3 → F
* Profile 4 → G

Each LED should flash BLUE for 500 ms.

---

## Pairing Mode

Verify:

* Correct profile LED blinks
* LED turns OFF after animation

---

## Host Status

Verify:

* Connected host → GREEN
* No host → RED

---

## Split Status

Verify:

* Right half connected → GREEN
* Right half disconnected → RED

---

## Battery Status

Validate:

* 95%
* 75%
* 55%
* 35%
* 15%

Ensure:

* Correct color
* Correct LED count
* Automatic timeout

---

# Future Enhancements (V2)

* Right-half synchronization
* Display both battery levels simultaneously
* Charging indication
* Low-battery warning animation
* Connection failure animations
* Dedicated status layer
* User-configurable animations
* User-configurable colors
* Runtime remapping of LED assignments

---

# Implementation Priority

1. Bluetooth profile indication
2. Bluetooth pairing indication
3. Host connection status
4. Split connection status
5. Battery status display

The implementation should prioritize maintainability, upstream compatibility, and low power consumption over visual complexity.
